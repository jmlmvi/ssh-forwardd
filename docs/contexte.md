# Contexte du projet ssh-forwardd

## Résumé

**ssh-forwardd** est un daemon macOS en C qui maintient des tunnels SSH en port forwarding avec reconnexion automatique. Projet terminé et en production.

## État actuel

- **Version** : 1.0 (commit `7bf6469`)
- **Repo** : https://github.com/jmlmvi/ssh-forwardd
- **Status** : En production sur ce Mac

## Fichiers du projet

```
ssh-forwardd/
├── src/ssh-forwardd.c          # Daemon C (~300 lignes)
├── Makefile                    # Build: make / sudo make install
├── README.md                   # Documentation (français)
├── .gitignore
├── docs/
│   ├── spec.md                 # Spécification technique
│   └── contexte.md             # Ce fichier
└── examples/
    ├── config-ssh-forwardd.conf.example
    └── eu.lmvi.ssh-forwardd.plist.example
```

## Installation sur ce Mac

| Élément | Emplacement |
|---------|-------------|
| Binaire | `/usr/local/bin/ssh-forwardd` |
| Config | `~/.ssh/config-ssh-forwardd.conf` |
| LaunchAgent | `~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist` |
| Logs | `/tmp/ssh-forwardd.log` |

## Tunnels configurés

| Port | Service | Destination |
|------|---------|-------------|
| 19002 | Kafka Services Monitor | test-kafka-prod:3000 |
| 19003 | CDC Reflet Kafka | test-vm-prod:3001 |
| 19004 | pgAdmin | odh-airflow-new-prod:5050 |

## Commandes utiles

```bash
# Voir les logs
tail -f /tmp/ssh-forwardd.log

# Vérifier le statut
launchctl list | grep ssh-forwardd

# Redémarrer le service
launchctl unload ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
launchctl load ~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist

# Tester les ports
nc -zv 127.0.0.1 19002
nc -zv 127.0.0.1 19003
nc -zv 127.0.0.1 19004
```

## Configuration sudoers

Pour permettre à Claude Code d'installer :
```
JMH ALL=(ALL) NOPASSWD: /usr/bin/make
```

## Points techniques importants

1. **Keepalive SSH** : Toutes les commandes utilisent `-o ServerAliveInterval=30 -o ServerAliveCountMax=3` pour détecter les connexions mortes en ~90s

2. **Tunnels zombies** : Si les tunnels ne répondent plus après veille/perte réseau, redémarrer le service (les keepalives évitent ce problème)

3. **Multi-cloud** : Compatible SSH standard, GCP (`gcloud compute ssh`) et AWS (`aws ssm`, bastion)

## Prochaines étapes possibles

- [ ] Ajouter des tunnels AWS quand nécessaire
- [ ] Créer une interface web de monitoring (optionnel)
- [ ] Ajouter un fichier LICENSE MIT

## Historique des commits

1. `493be59` - Initial release
2. `841caa4` - Traduire README en français
3. `564fa03` - Ajouter options keepalive SSH
4. `7bf6469` - Ajouter support AWS et documentation multi-cloud
