# ssh-forwardd

Un daemon macOS léger qui maintient des tunnels SSH en port forwarding avec reconnexion automatique.

## Fonctionnalités

- **Reconnexion automatique** — Relance les tunnels après une coupure réseau
- **Sortie de veille** — Recrée les tunnels après le réveil du Mac
- **Support SSH & GCP** — Compatible avec `ssh` et `gcloud compute ssh`
- **Backoff exponentiel** — Évite les tempêtes de connexion (1s → 60s)
- **Intégration launchd** — Démarre automatiquement à la connexion

## Prérequis

- macOS (Intel ou Apple Silicon)
- Compilateur Clang/GCC
- Clés SSH configurées (ou gcloud auth)

## Installation

### Compiler depuis les sources

```bash
git clone https://github.com/jmlmvi/ssh-forwardd.git
cd ssh-forwardd
make
sudo make install
```

### Configurer les tunnels

Créer le fichier de configuration :

```bash
nano ~/.ssh/config-ssh-forwardd.conf
```

Exemple de configuration :

```bash
# Tunnel SSH simple
ssh -N -L 19000:localhost:5432 user@db.example.com

# SSH via jump host
ssh -N -L 19001:internal-db:3306 -J bastion.example.com user@server

# Tunnel GCP
gcloud compute ssh my-vm \
  --project=my-project \
  --zone=europe-west1-b \
  -- -N -L 19002:localhost:8080
```

**Règles pour chaque ligne :**
- Doit inclure `-N` (pas de shell)
- Doit inclure au moins un `-L` (port forwarding local)
- Utiliser les ports 19000-19999 pour éviter les conflits

### Installer le LaunchAgent

```bash
cp examples/eu.lmvi.ssh-forwardd.plist ~/Library/LaunchAgents/
```

Modifier le plist si nécessaire pour correspondre à votre nom d'utilisateur et chemins.

### Démarrer le service

```bash
launchctl load ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
```

## Utilisation

### Vérifier le statut

```bash
launchctl list | grep ssh-forwardd
```

### Voir les logs

```bash
tail -f /tmp/ssh-forwardd.log
```

### Redémarrer après modification de la config

```bash
launchctl unload ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
launchctl load ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
```

### Arrêter le service

```bash
launchctl unload ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
```

## Désinstallation

```bash
launchctl unload ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
rm ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
sudo make uninstall
rm ~/.ssh/config-ssh-forwardd.conf
```

## Fonctionnement

1. Lit les commandes de tunnel depuis `~/.ssh/config-ssh-forwardd.conf`
2. Lance chaque tunnel comme processus enfant
3. Surveille les processus et les relance s'ils meurent
4. Utilise un backoff exponentiel (1s à 60s) en cas d'échecs répétés
5. Gère SIGTERM/SIGINT pour un arrêt propre

## Limitations

- Port forwarding local uniquement (`-L`)
- Pas de tunnels inverses (`-R`)
- Pas de proxy SOCKS (`-D`)
- Pas de gestion de clés intégrée (utilise SSH/gcloud du système)

## Licence

Licence MIT - Voir [LICENSE](LICENSE) pour plus de détails.
