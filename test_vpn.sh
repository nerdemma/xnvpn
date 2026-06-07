#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/src"
REPORT="$ROOT/vpn_test_report.txt"
SERVER_LOG="$ROOT/vpn_server.log"
CLIENT_LOG="$ROOT/vpn_client.log"
PING_LOG="$ROOT/vpn_ping.log"
SERVER_BIN="$SRC/server"
CLIENT_BIN="$SRC/client"
SERVER_PID=0
CLIENT_PID=0

cleanup() {
    echo "\n[cleanup] stopping processes and removing test interfaces" >> "$REPORT"
    if [[ $CLIENT_PID -ne 0 ]]; then
        kill "$CLIENT_PID" 2>/dev/null || true
        wait "$CLIENT_PID" 2>/dev/null || true
    fi
    if [[ $SERVER_PID -ne 0 ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    ip link del tun0 2>/dev/null || true
    ip link del tun1 2>/dev/null || true
}
trap cleanup EXIT

if [[ $(id -u) -ne 0 ]]; then
    echo "Este script debe ejecutarse con root: sudo $0"
    exit 1
fi

: > "$REPORT"
: > "$SERVER_LOG"
: > "$CLIENT_LOG"
: > "$PING_LOG"

echo "VPN Prototype Functional Test" >> "$REPORT"
echo "Fecha: $(date)" >> "$REPORT"
echo "Directorio del proyecto: $ROOT" >> "$REPORT"
echo "" >> "$REPORT"

echo "1. Compilando los binarios..." | tee -a "$REPORT"
if gcc -Wall -Wextra -o "$CLIENT_BIN" "$SRC/client.c" 2>> "$REPORT" && \
   gcc -Wall -Wextra -o "$SERVER_BIN" "$SRC/server.c" 2>> "$REPORT"; then
    echo "Compilación exitosa." | tee -a "$REPORT"
else
    echo "Compilación fallida." | tee -a "$REPORT"
    exit 1
fi

echo "\n2. Iniciando el servidor..." | tee -a "$REPORT"
"$SERVER_BIN" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!

sleep 1

if ps -p "$SERVER_PID" >/dev/null 2>&1; then
    echo "Servidor iniciado (PID $SERVER_PID)." | tee -a "$REPORT"
else
    echo "No se pudo iniciar el servidor." | tee -a "$REPORT"
    cat "$SERVER_LOG" >> "$REPORT"
    exit 1
fi

echo "\n3. Iniciando el cliente..." | tee -a "$REPORT"
"$CLIENT_BIN" 127.0.0.1 > "$CLIENT_LOG" 2>&1 &
CLIENT_PID=$!

sleep 1

if ps -p "$CLIENT_PID" >/dev/null 2>&1; then
    echo "Cliente iniciado (PID $CLIENT_PID)." | tee -a "$REPORT"
else
    echo "No se pudo iniciar el cliente." | tee -a "$REPORT"
    cat "$CLIENT_LOG" >> "$REPORT"
    exit 1
fi

echo "\n4. Verificando interfaces TUN..." | tee -a "$REPORT"
for iface in tun0 tun1; do
    if ip addr show "$iface" >/dev/null 2>&1; then
        echo "$iface encontrado:" | tee -a "$REPORT"
        ip addr show "$iface" | sed 's/^/    /' >> "$REPORT"
    else
        echo "ERROR: $iface no existe." | tee -a "$REPORT"
        exit 1
    fi
done

sleep 1

echo "\n5. Ejecutando pings de prueba..." | tee -a "$REPORT"
{
    echo "Ping desde tun1 hacia 10.0.0.1"
    ping -I tun1 -c 3 10.0.0.1
    echo "\nPing desde tun0 hacia 10.0.0.2"
    ping -I tun0 -c 3 10.0.0.2
} > "$PING_LOG" 2>&1

cat "$PING_LOG" >> "$REPORT"

echo "\n6. Resumen de resultados" | tee -a "$REPORT"
PING1_OK=0
PING2_OK=0
if grep -q "3 received" "$PING_LOG"; then
    PING1_OK=1
fi
if grep -q "3 received" "$PING_LOG"; then
    PING2_OK=1
fi

if [[ $PING1_OK -eq 1 && $PING2_OK -eq 1 ]]; then
    echo "RESULTADO: Éxito. El túnel TUN funciona correctamente y el tráfico ICMP atraviesa el túnel." | tee -a "$REPORT"
else
    echo "RESULTADO: Fallo parcial o total. Revisa los registros y las interfaces." | tee -a "$REPORT"
fi

echo "" >> "$REPORT"
echo "Registros guardados en:" >> "$REPORT"
echo "  - $SERVER_LOG" >> "$REPORT"
echo "  - $CLIENT_LOG" >> "$REPORT"
echo "  - $PING_LOG" >> "$REPORT"
echo "Reporte final: $REPORT" >> "$REPORT"

echo "\nPrueba completada. Revisa $REPORT para el detalle."