# SPECIFICATION — ssh-forwardd

## 1. Objectif

**ssh-forwardd** est un daemon macOS chargé de maintenir exclusivement des tunnels SSH en port forwarding, avec :

- Reconnexion automatique en cas de coupure réseau
- Recréation automatique des tunnels après une sortie de veille du Mac
- Support des tunnels SSH classiques et via GCP (`gcloud compute ssh`)
- Aucun serveur SSH embarqué, aucune connexion entrante
- Fonctionnement en arrière-plan via launchd

---

## 2. Périmètre fonctionnel

### Inclus

- Port forwarding SSH (`-L` uniquement)
- Supervision des tunnels
- Reconnexion automatique
- Lecture d'un fichier de configuration dédié
- Support des commandes SSH non standards (ex: `gcloud compute ssh`)
- Fonctionnement en user-space (pas root)

### Exclu

- Pas de shell distant
- Pas de SCP / SFTP
- Pas de reverse port (`-R`)
- Pas de dynamic proxy (`-D`)
- Pas de gestion de clés (déléguée à SSH / gcloud)

---

## 3. Architecture générale

```
┌───────────────┐
│ launchd       │
│ (LaunchAgent) │
└───────┬───────┘
        │
        ▼
┌──────────────────┐
│ ssh-forwardd     │
│ (daemon C)       │
│                  │
│ - lit config     │
│ - lance tunnels  │
│ - supervise ssh  │
│ - relance si KO  │
└───────┬──────────┘
        │
        ▼
┌──────────────────┐
│ ssh / gcloud     │
│ (process enfant) │
└──────────────────┘
```

---

## 4. Fichier de configuration

### Emplacement

```
~/.ssh/config-ssh-forwardd.conf
```

### Format du fichier

- Une commande par tunnel
- Une ligne = un tunnel
- Commentaires autorisés avec `#`
- Les commandes sont exécutées telles quelles
- **Obligation** :
  - Inclure `-N`
  - Inclure au moins un `-L`

### Convention de ports

- Plage réservée : **19000 - 19999**
- Permet d'éviter les conflits avec les ports système et applicatifs courants

### Exemple de configuration

```bash
# Tunnel SSH simple vers un serveur
ssh -N -L 19000:localhost:5432 user@db.example.com

# Tunnel SSH avec jump host
ssh -N -L 19001:internal-db:3306 -J bastion.example.com user@internal-server

# Tunnel GCP vers test-kafka-prod
gcloud compute ssh test-kafka-prod \
  --project=amarena-main-prod \
  --zone=europe-west1-b \
  -- -N -L 19002:localhost:3000

# Tunnel GCP vers test-vm-prod
gcloud compute ssh test-vm-prod \
  --project=amarena-main-prod \
  --zone=europe-west1-b \
  -- -N -L 19003:localhost:3001
```

---

## 5. Comportement du daemon

### Démarrage

1. Lecture du fichier `config-ssh-forwardd.conf`
2. Parsing des commandes valides
3. Lancement de chaque tunnel dans un process enfant
4. Enregistrement des PID

### Supervision

- Le daemon surveille chaque process enfant
- Si un process SSH se termine :
  - Tentative de reconnexion automatique
  - Backoff exponentiel (1s → 60s max)
- Aucune duplication de tunnel autorisée

### Gestion veille / réveil

- À la sortie de veille :
  - Les sockets sont considérés invalides
  - Les tunnels sont relancés automatiquement
- Aucune action utilisateur requise

---

## 6. Contraintes techniques

### Langage

- C (ANSI / POSIX)

### OS cible

- macOS (Intel + Apple Silicon)

### Dépendances

- Aucune librairie externe obligatoire
- Appel direct aux binaires :
  - `ssh`
  - `gcloud`

---

## 7. Sécurité

- Exécution en utilisateur standard
- Connexions sortantes uniquement
- Respect des clés SSH existantes
- Pas de stockage de secrets
- Compatible avec SSH Agent et `gcloud auth`

---

## 8. Logs

### Destination

- `stdout` / `stderr`
- Capturés par launchd

### Niveau minimal attendu

- `start tunnel`
- `stop tunnel`
- `reconnect`
- `error exit code`

---

## 9. Intégration launchd (résumé)

Le daemon est lancé via un **LaunchAgent** :

- Démarrage automatique à la connexion utilisateur
- Redémarrage automatique si crash
- Persistant après veille

### Emplacement recommandé

```
~/Library/LaunchAgents/eu.lmvi.ssh-forwardd.plist
```

---

## 10. Objectif final

À l'issue :

- Les ports locaux (plage 19000-19999) sont toujours disponibles, par exemple :
  - `127.0.0.1:19000` → `db.example.com:5432` (SSH simple)
  - `127.0.0.1:19001` → `internal-db:3306` (SSH via jump host)
  - `127.0.0.1:19002` → `test-kafka-prod:3000` (gcloud)
  - `127.0.0.1:19003` → `test-vm-prod:3001` (gcloud)
- Aucune action manuelle
- Fonctionne après reboot, veille, perte réseau
- Transparent pour les applications (pgAdmin, Electron, navigateur, etc.)

---

## Prochaines étapes possibles

- Le fichier `.plist` launchd prêt à copier
- Le pseudo-code C du superviseur
- Un Makefile minimal
- Une API locale (status / healthcheck) pour UI Electron
