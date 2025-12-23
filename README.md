# ssh-forwardd

A lightweight macOS daemon that maintains SSH port forwarding tunnels with automatic reconnection.

## Features

- **Automatic reconnection** — Restarts tunnels after network disconnection
- **Wake from sleep** — Recreates tunnels after Mac wakes from sleep
- **SSH & GCP support** — Works with standard `ssh` and `gcloud compute ssh`
- **Exponential backoff** — Prevents connection storms (1s → 60s)
- **launchd integration** — Starts automatically at login

## Requirements

- macOS (Intel or Apple Silicon)
- Clang/GCC compiler
- SSH keys configured (or gcloud auth)

## Installation

### Build from source

```bash
git clone https://github.com/yourusername/ssh-forwardd.git
cd ssh-forwardd
make
sudo make install
```

### Configure tunnels

Create your tunnel configuration:

```bash
nano ~/.ssh/config-ssh-forwardd.conf
```

Example configuration:

```bash
# Simple SSH tunnel
ssh -N -L 19000:localhost:5432 user@db.example.com

# SSH through jump host
ssh -N -L 19001:internal-db:3306 -J bastion.example.com user@server

# GCP tunnel
gcloud compute ssh my-vm \
  --project=my-project \
  --zone=europe-west1-b \
  -- -N -L 19002:localhost:8080
```

**Requirements for each line:**
- Must include `-N` (no shell)
- Must include at least one `-L` (local port forward)
- Use ports 19000-19999 to avoid conflicts

### Install LaunchAgent

```bash
cp examples/eu.lmvi.ssh-forwardd.plist ~/Library/LaunchAgents/
```

Edit the plist if needed to match your username and paths.

### Start the service

```bash
launchctl load ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
```

## Usage

### Check status

```bash
launchctl list | grep ssh-forwardd
```

### View logs

```bash
tail -f /tmp/ssh-forwardd.log
```

### Restart after config change

```bash
launchctl unload ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
launchctl load ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
```

### Stop the service

```bash
launchctl unload ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
```

## Uninstall

```bash
launchctl unload ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
rm ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
sudo make uninstall
rm ~/.ssh/config-ssh-forwardd.conf
```

## How it works

1. Reads tunnel commands from `~/.ssh/config-ssh-forwardd.conf`
2. Spawns each tunnel as a child process
3. Monitors processes and restarts them if they die
4. Uses exponential backoff (1s to 60s) on repeated failures
5. Handles SIGTERM/SIGINT for clean shutdown

## Limitations

- Local port forwarding only (`-L`)
- No reverse tunnels (`-R`)
- No SOCKS proxy (`-D`)
- No built-in key management (uses system SSH/gcloud)

## License

MIT License - See [LICENSE](LICENSE) for details.
