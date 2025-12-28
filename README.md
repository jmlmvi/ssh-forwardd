# ssh-forwardd

Un daemon macOS léger qui maintient des tunnels SSH en port forwarding avec reconnexion automatique.

## Fonctionnalités

- **Reconnexion automatique** — Relance les tunnels après une coupure réseau
- **Sortie de veille** — Recrée les tunnels après le réveil du Mac
- **Multi-cloud** — Compatible avec SSH standard, GCP et AWS
- **Backoff exponentiel** — Évite les tempêtes de connexion (1s → 60s)
- **Intégration launchd** — Démarre automatiquement à la connexion

## Prérequis

- macOS (Intel ou Apple Silicon)
- Compilateur Clang/GCC
- Clés SSH configurées

### Outils cloud (optionnels)

| Provider | Outil requis | Installation |
|----------|--------------|--------------|
| GCP | `gcloud` CLI | [cloud.google.com/sdk](https://cloud.google.com/sdk/docs/install) |
| AWS | `aws` CLI + Session Manager plugin | [aws.amazon.com/cli](https://aws.amazon.com/cli/) |

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

### Exemples de configuration

#### SSH standard

```bash
# Tunnel SSH simple
ssh -N -o ServerAliveInterval=30 -o ServerAliveCountMax=3 \
  -L 19000:localhost:5432 user@db.example.com

# SSH via bastion (jump host)
ssh -N -o ServerAliveInterval=30 -o ServerAliveCountMax=3 \
  -J bastion.example.com \
  -L 19001:internal-db:3306 user@private-server
```

#### Google Cloud Platform (GCP)

```bash
# Tunnel via gcloud compute ssh
gcloud compute ssh my-vm \
  --project=my-project \
  --zone=europe-west1-b \
  -- -N -o ServerAliveInterval=30 -o ServerAliveCountMax=3 -L 19002:localhost:8080
```

#### Amazon Web Services (AWS)

```bash
# SSH direct vers EC2
ssh -N -o ServerAliveInterval=30 -o ServerAliveCountMax=3 \
  -i ~/.ssh/aws-key.pem \
  -L 19010:localhost:5432 ec2-user@ec2-xx-xx-xx-xx.compute.amazonaws.com

# SSH via bastion AWS
ssh -N -o ServerAliveInterval=30 -o ServerAliveCountMax=3 \
  -J ec2-user@bastion.example.com \
  -i ~/.ssh/aws-key.pem \
  -L 19011:rds-instance.xxx.region.rds.amazonaws.com:5432 ec2-user@private-instance

# AWS SSM Session Manager (sans accès SSH direct)
aws ssm start-session \
  --target i-0123456789abcdef \
  --document-name AWS-StartPortForwardingSession \
  --parameters '{"portNumber":["5432"],"localPortNumber":["19012"]}'
```

### Règles de configuration

- Doit inclure `-N` (pas de shell)
- Doit inclure au moins un `-L` (port forwarding local)
- Utiliser les ports 19000-19999 pour éviter les conflits
- **Recommandé** : Ajouter les options keepalive pour détecter les connexions mortes :
  - `-o ServerAliveInterval=30` (ping toutes les 30s)
  - `-o ServerAliveCountMax=3` (ferme après 3 échecs = 90s)

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
- Pas de gestion de clés intégrée (utilise SSH/gcloud/aws du système)

## Licence

Licence MIT - Voir [LICENSE](LICENSE) pour plus de détails.
