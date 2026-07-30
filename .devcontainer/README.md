# FreeRTOS NTP DevContainer

Dieser DevContainer bietet eine vollständige Entwicklungsumgebung für das FreeRTOS NTP Client Projekt mit integriertem Chrony NTP Server.

## Features

- **Ubuntu 24.04** als Basis
- **Chrony NTP Server** für lokales Testing
- **Build-Tools**: gcc, make, cmake
- **FreeRTOS Kernel** und **FreeRTOS-Plus-TCP** automatisch geklont
- **Debugging-Tools**: gdb, valgrind, strace
- **Network-Tools**: tcpdump, net-tools
- **SSL/TLS Bibliotheken**: OpenSSL, mbedTLS (für NTS Support)
- **Common Utilities**: zsh, Oh My Zsh, und weitere nützliche Tools
- **CI/CD**: GitHub Actions Workflow für automatische Tests mit Chrony

## Verwendung

### Container starten

1. Öffne das Projekt in VS Code
2. Drücke `F1` und wähle `Dev Containers: Reopen in Container`
3. Warte, bis der Container erstellt und gestartet wurde
4. Beim Verbinden wird eine Hilfe-Nachricht angezeigt

### Chrony NTP Server

Der Chrony NTP Server muss **manuell gestartet** werden:

```bash
# Chrony Server starten
.devcontainer/start-chrony.sh
```

Nach dem Start ist der Server auf Port 123 (UDP) verfügbar.

#### Status überprüfen

```bash
# Synchronisationsstatus anzeigen
chronyc tracking

# NTP Quellen anzeigen
chronyc sources

# Detaillierte Quellinformationen
chronyc sourcestats
```

#### Chrony neu starten

```bash
sudo systemctl restart chrony
# oder
/usr/local/bin/start-chrony.sh
```

### FreeRTOS NTP Client für lokalen Server konfigurieren

Um den lokalen Chrony Server von Ihrer FreeRTOS Anwendung zu nutzen, konfigurieren Sie den NTP Client wie folgt:

```c
#include "freertos_ntp.h"

// NTP Konfiguration für lokalen Chrony Server
NTPTaskConfig_t xNTPConfig = {
    .pcNTPServer = "127.0.0.1",        // Lokaler Chrony Server
    .usNTPPort = 123,                   // Standard NTP Port
    .ulPollIntervalSec = 60,           // Abfrageintervall in Sekunden
    .ulSyncThresholdUs = 1000000,      // 1 Sekunde Schwellwert
    .xSkewThresholdUs = 100000,        // 100ms Skew-Schwellwert
    .pxTimeSetCallback = prvTimeSetCallback,
    .pxSkewSetCallback = prvSkewSetCallback,
    .ucMaxRetries = 3,
    .usTimeoutMs = 5000
};

// NTP Task erstellen
xNTPHandle = xNTPTaskCreate(&xNTPConfig);
```

**Wichtig:** Stellen Sie sicher, dass:
- Der Chrony Server läuft (siehe oben)
- Die FreeRTOS-Plus-TCP Netzwerk-Stack konfiguriert ist
- Callback-Funktionen implementiert sind (siehe examples/x86_test/main.c)

### FreeRTOS x86 Beispiel kompilieren

```bash
# In das Beispielverzeichnis wechseln
cd examples/x86_test

# Kompilieren mit Make
make

# Oder mit CMake (vom Hauptverzeichnis)
mkdir -p build
cd build
cmake .. \
  -DFREERTOS_KERNEL_PATH=/workspace/FreeRTOS-Kernel \
  -DFREERTOS_PLUS_TCP_PATH=/workspace/FreeRTOS-Plus-TCP \
  -DFREERTOS_PORT=GCC/Posix \
  -DBUILD_NTP_EXAMPLE=ON
make
```

**Ausführliche Anleitung:** Siehe [examples/x86_test/README.md](../../examples/x86_test/README.md)

### Bibliothek bauen

```bash
# Im Hauptverzeichnis mit Make
make

# Oder mit CMake (nur Bibliothek)
mkdir -p build
cd build
cmake .. \
  -DFREERTOS_KERNEL_PATH=/workspace/FreeRTOS-Kernel \
  -DFREERTOS_PLUS_TCP_PATH=/workspace/FreeRTOS-Plus-TCP \
  -DFREERTOS_PORT=GCC/Posix
make

# Mit CMake (Bibliothek und Beispiel)
mkdir -p build
cd build
cmake .. \
  -DFREERTOS_KERNEL_PATH=/workspace/FreeRTOS-Kernel \
  -DFREERTOS_PLUS_TCP_PATH=/workspace/FreeRTOS-Plus-TCP \
  -DFREERTOS_PORT=GCC/Posix \
  -DBUILD_NTP_EXAMPLE=ON
make
```

### x86 Beispiel ausführen

```bash
# Stelle sicher, dass Chrony läuft
.devcontainer/start-chrony.sh

# Mit Make gebaut
cd examples/x86_test
./ntp_test

# Mit CMake gebaut
cd build/examples/x86_test
./ntp_test
```

## Umgebungsvariablen

Der Container setzt automatisch folgende Umgebungsvariablen:

- `FREERTOS_PATH=/workspace/FreeRTOS-Kernel`
- `FREERTOS_TCP_PATH=/workspace/FreeRTOS-Plus-TCP`
- `FREERTOS_PORT=GCC/Posix`

## NTP Testing

### Lokale NTP Abfrage

```bash
# Mit ntpdate (muss ggf. installiert werden)
sudo apt-get update && sudo apt-get install -y ntpdate
ntpdate -q localhost

# Mit chronyc
chronyc tracking
```

### NTP Server von außerhalb ansprechen

Wenn der Container läuft, ist der NTP Server auf Port 123 verfügbar:

```bash
# Von Host-System
ntpdate -q localhost
```

## Troubleshooting

### Chrony startet nicht

```bash
# Logs überprüfen
sudo journalctl -u chrony -n 50

# Manuell mit Debug-Output starten
sudo chronyd -d -f /etc/chrony/chrony.conf
```

### Port 123 bereits belegt

Falls Port 123 bereits auf dem Host-System verwendet wird, ändere in `devcontainer.json`:

```json
"forwardPorts": [],
```

und greife auf den NTP Server über die Container-IP zu.

### Berechtigungen

Der Container benötigt erweiterte Capabilities für NTP:
- `SYS_TIME`: Zum Setzen der Systemzeit
- `NET_ADMIN`: Für Netzwerk-Operationen
- `NET_RAW`: Für Raw Sockets

Diese sind bereits in der `devcontainer.json` konfiguriert.

## GitHub Actions CI/CD

Das Projekt enthält einen automatisierten Test-Workflow (`.github/workflows/test-with-chrony.yml`), der:

1. **Library Build Test**: 
   - Baut die Bibliothek mit Make und CMake
   - Verifiziert die Build-Artefakte

2. **x86 Example Test**:
   - Installiert und konfiguriert Chrony NTP Server
   - Baut das x86 Test-Beispiel
   - Führt das Beispiel gegen den laufenden Chrony Server aus
   - Überprüft Chrony Statistiken nach dem Test

3. **Lint Check**:
   - Prüft Code-Formatierung und Stil

Der Workflow läuft automatisch bei:
- Pushes auf `main`, `develop` oder `copilot/**` Branches
- Pull Requests nach `main`
- Manueller Trigger über GitHub UI

### Workflow lokal testen

```bash
# Installiere act (GitHub Actions local runner)
# https://github.com/nektos/act

# Führe einen Job lokal aus
act -j test-library

# Führe den gesamten Workflow aus
act push
```

## Weitere Informationen

- [Chrony Dokumentation](https://chrony.tuxfamily.org/documentation.html)
- [FreeRTOS Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel)
- [FreeRTOS-Plus-TCP](https://github.com/FreeRTOS/FreeRTOS-Plus-TCP)
