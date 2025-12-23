/*
 * ssh-forwardd - SSH tunnel supervisor daemon for macOS
 * Maintains SSH port forwarding tunnels with automatic reconnection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>

#define MAX_TUNNELS 64
#define MAX_CMD_LEN 4096
#define MAX_ARGS 128
#define CONFIG_FILENAME ".ssh/config-ssh-forwardd.conf"

#define MIN_BACKOFF 1
#define MAX_BACKOFF 60

typedef struct {
    char command[MAX_CMD_LEN];
    pid_t pid;
    int backoff;
    time_t last_start;
    int active;
} Tunnel;

static Tunnel tunnels[MAX_TUNNELS];
static int tunnel_count = 0;
static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t child_exited = 0;

static void log_msg(const char *level, const char *fmt, ...) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(stderr, "[%s] [%s] ", timestamp, level);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    fflush(stderr);
}

static void sigchld_handler(int sig) {
    (void)sig;
    child_exited = 1;
}

static void sigterm_handler(int sig) {
    (void)sig;
    running = 0;
}

static char *get_config_path(void) {
    static char path[1024];
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return NULL;

    snprintf(path, sizeof(path), "%s/%s", home, CONFIG_FILENAME);
    return path;
}

static int parse_config(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        log_msg("ERROR", "Cannot open config file: %s", path);
        return -1;
    }

    char line[MAX_CMD_LEN];
    char cmd_buffer[MAX_CMD_LEN] = "";
    int in_continuation = 0;

    while (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        // Skip empty lines and comments (only if not in continuation)
        if (!in_continuation) {
            char *trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            if (*trimmed == '\0' || *trimmed == '#') continue;
        }

        // Check for line continuation
        if (len > 0 && line[len-1] == '\\') {
            line[len-1] = ' ';
            strcat(cmd_buffer, line);
            in_continuation = 1;
            continue;
        }

        // Complete the command
        strcat(cmd_buffer, line);

        // Validate command has -N and -L
        if (strstr(cmd_buffer, "-N") == NULL) {
            log_msg("WARN", "Skipping command without -N: %.50s...", cmd_buffer);
            cmd_buffer[0] = '\0';
            in_continuation = 0;
            continue;
        }
        if (strstr(cmd_buffer, "-L") == NULL) {
            log_msg("WARN", "Skipping command without -L: %.50s...", cmd_buffer);
            cmd_buffer[0] = '\0';
            in_continuation = 0;
            continue;
        }

        // Store tunnel
        if (tunnel_count < MAX_TUNNELS) {
            strncpy(tunnels[tunnel_count].command, cmd_buffer, MAX_CMD_LEN - 1);
            tunnels[tunnel_count].pid = 0;
            tunnels[tunnel_count].backoff = MIN_BACKOFF;
            tunnels[tunnel_count].last_start = 0;
            tunnels[tunnel_count].active = 1;
            tunnel_count++;
            log_msg("INFO", "Loaded tunnel: %.60s...", cmd_buffer);
        } else {
            log_msg("WARN", "Max tunnels reached, ignoring: %.50s...", cmd_buffer);
        }

        cmd_buffer[0] = '\0';
        in_continuation = 0;
    }

    fclose(fp);
    log_msg("INFO", "Loaded %d tunnel(s) from config", tunnel_count);
    return tunnel_count;
}

static pid_t start_tunnel(Tunnel *t) {
    pid_t pid = fork();

    if (pid < 0) {
        log_msg("ERROR", "Fork failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        // Child process
        // Parse command into arguments
        char *args[MAX_ARGS];
        char cmd_copy[MAX_CMD_LEN];
        strncpy(cmd_copy, t->command, MAX_CMD_LEN - 1);
        cmd_copy[MAX_CMD_LEN - 1] = '\0';

        int argc = 0;
        char *token = strtok(cmd_copy, " \t");
        while (token && argc < MAX_ARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " \t");
        }
        args[argc] = NULL;

        if (argc == 0) {
            _exit(1);
        }

        execvp(args[0], args);
        // If exec fails
        fprintf(stderr, "Failed to exec %s: %s\n", args[0], strerror(errno));
        _exit(127);
    }

    // Parent process
    t->pid = pid;
    t->last_start = time(NULL);
    log_msg("INFO", "Started tunnel (PID %d): %.60s...", pid, t->command);
    return pid;
}

static void stop_tunnel(Tunnel *t) {
    if (t->pid > 0) {
        log_msg("INFO", "Stopping tunnel (PID %d)", t->pid);
        kill(t->pid, SIGTERM);

        // Give it a moment to terminate gracefully
        usleep(100000); // 100ms

        // Force kill if still running
        if (kill(t->pid, 0) == 0) {
            kill(t->pid, SIGKILL);
        }
        t->pid = 0;
    }
}

static void stop_all_tunnels(void) {
    log_msg("INFO", "Stopping all tunnels...");
    for (int i = 0; i < tunnel_count; i++) {
        stop_tunnel(&tunnels[i]);
    }
}

static void reap_children(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Find which tunnel this was
        for (int i = 0; i < tunnel_count; i++) {
            if (tunnels[i].pid == pid) {
                if (WIFEXITED(status)) {
                    log_msg("WARN", "Tunnel exited (PID %d) with code %d",
                            pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    log_msg("WARN", "Tunnel killed (PID %d) by signal %d",
                            pid, WTERMSIG(status));
                }
                tunnels[i].pid = 0;
                break;
            }
        }
    }
}

static void supervise_tunnels(void) {
    time_t now = time(NULL);

    for (int i = 0; i < tunnel_count; i++) {
        Tunnel *t = &tunnels[i];

        if (!t->active) continue;

        // Check if tunnel needs to be started/restarted
        if (t->pid == 0) {
            // Check backoff
            if (now - t->last_start < t->backoff) {
                continue;
            }

            // Start tunnel
            if (start_tunnel(t) > 0) {
                // Reset backoff on successful start
                // (will increase again if it dies quickly)
            } else {
                // Increase backoff on failure
                t->backoff = (t->backoff * 2 > MAX_BACKOFF) ? MAX_BACKOFF : t->backoff * 2;
            }
        } else {
            // Tunnel is running, check if process died quickly (< 5s)
            // If so, increase backoff; otherwise reset it
            if (t->last_start > 0 && now - t->last_start > 5) {
                t->backoff = MIN_BACKOFF;
            }
        }
    }
}

static void check_tunnel_health(void) {
    for (int i = 0; i < tunnel_count; i++) {
        Tunnel *t = &tunnels[i];
        if (t->pid > 0) {
            // Check if process is still alive
            if (kill(t->pid, 0) != 0) {
                log_msg("WARN", "Tunnel process %d no longer exists", t->pid);
                t->pid = 0;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    log_msg("INFO", "ssh-forwardd starting...");

    // Setup signal handlers
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Load configuration
    char *config_path = get_config_path();
    if (!config_path) {
        log_msg("ERROR", "Cannot determine config path");
        return 1;
    }

    log_msg("INFO", "Loading config from: %s", config_path);

    if (parse_config(config_path) <= 0) {
        log_msg("ERROR", "No valid tunnels found in config");
        return 1;
    }

    // Start all tunnels
    for (int i = 0; i < tunnel_count; i++) {
        start_tunnel(&tunnels[i]);
        usleep(100000); // Small delay between starts
    }

    // Main supervision loop
    log_msg("INFO", "Entering supervision loop...");

    while (running) {
        // Handle any exited children
        if (child_exited) {
            child_exited = 0;
            reap_children();
        }

        // Periodic health check
        check_tunnel_health();

        // Restart any dead tunnels
        supervise_tunnels();

        // Sleep before next iteration
        sleep(1);
    }

    // Cleanup
    log_msg("INFO", "Shutting down...");
    stop_all_tunnels();

    // Final reap
    reap_children();

    log_msg("INFO", "ssh-forwardd stopped.");
    return 0;
}
