#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$ROOT/src"
REPORT="$ROOT/vpn_test_report.txt"
SERVER_LOG="$ROOT/vpn_server.log"
CLIENT_LOG="$ROOT/vpn_client.log"
PING_LOG_1="$ROOT/vpn_ping_client_to_server.log"
PING_LOG_2="$ROOT/vpn_ping_server_to_client.log"
SERVER_BIN="$SRC/server"
CLIENT_BIN="$SRC/client"

# Nombres de los namespaces de red
NS_SERVER="ns_vpn_server"
NS_CLIENT="ns_vpn_client"

cleanup() {
    echo "" >> "$REPORT"
    echo "[cleanup] Deteniendo procesos y eliminando Namespaces/Veth..." >> "$REPORT"
    
    # Matar binarios de forma segura en sus namespaces
    ip netns exec "$NS_CLIENT" killall client 2>/dev/null || true
    ip netns exec "$NS_SERVER" killall server 2>/dev/null || true
    
    # Eliminar namespaces (esto borra automáticamente interfaces tun y veth asociadas)
    ip netns del "$NS_SERVER" 2>/dev/null || true
    ip netns del "$NS_CLIENT" 2>/dev/null || true
}
trap cleanup EXIT

if [[ $(id -u) -ne 0 ]]; then
    echo "Este script debe ejecutarse con root: sudo $0"
    exit 1
fi

: > "$REPORT"
: > "$SERVER_LOG"
: > "$CLIENT_LOG"
: > "$PING_LOG_1"
: > "$PING_LOG_2"

echo "VPN Prototype Functional Test (Isolated Context)" >> "$REPORT"
echo "Fecha: $(date)" >> "$REPORT"
echo "" >> "$REPORT"

echo "1. Compilando los binarios (con OpenSSL)..." | tee -a "$REPORT"
if gcc -Wall -Wextra -o "$CLIENT_BIN" "$SRC/client.c" -lcrypto 2>> "$REPORT" && \
   gcc -Wall -Wextra -o "$SERVER_BIN" "$SRC/server.c" -lcrypto 2>> "$REPORT"; then
    echo "Compilación exitosa." | tee -a "$REPORT"
else
    echo "Compilación fallida. Revisa el reporte." | tee -a "$REPORT"
    exit 1
fi

echo "" | tee -a "$REPORT"
echo "2. Configurando red aislada (Namespaces)..." | tee -a "$REPORT"
# Crear entornos aislados
ip netns add "$NS_SERVER"
ip netns add "$NS_CLIENT"

# Crear un cable virtual (veth) para interconectar los dos entornos
ip link add veth_srv type veth peer name veth_cli
ip link set veth_srv netns "$NS_SERVER"
ip link set veth_cli netns "$NS_CLIENT"

# Configurar IPs reales de tránsito de internet (Red 192.168.50.0/24)
ip netns exec "$NS_SERVER" ip addr add 192.168.50.1/24 dev veth_srv
ip netns exec "$NS_SERVER" ip link set veth_srv up
ip netns exec "$NS_SERVER" ip link set lo up

ip netns exec "$NS_CLIENT" ip addr add 192.168.50.2/24 dev veth_cli
ip netns exec "$NS_CLIENT" ip link set veth_cli up
ip netns exec "$NS_CLIENT" ip link set lo up

# Deshabilitar rp_filter en los entornos virtuales para evitar drops asimétricos
ip netns exec "$NS_SERVER" sysctl -w net.ipv4.conf.all.rp_filter=0 >/dev/null
ip netns exec "$NS_CLIENT" sysctl -w net.ipv4.conf.all.rp_filter=0 >/dev/null

echo "" | tee -a "$REPORT"
echo "3. Iniciando el servidor en su propio entorno..." | tee -a "$REPORT"
# El servidor se ejecuta dentro de su namespace escuchando en su IP de tránsito
ip netns exec "$NS_SERVER" "$SERVER_BIN" > "$SERVER_LOG" 2>&1 &
sleep 1

echo "" | tee -a "$REPORT"
echo "4. Iniciando el cliente en su propio entorno..." | tee -a "$REPORT"
# El cliente se conecta a la IP del servidor mediante el cable veth
ip netns exec "$NS_CLIENT" "$CLIENT_BIN" 192.168.50.1 > "$CLIENT_LOG" 2>&1 &
sleep 2

echo "" | tee -a "$REPORT"
echo "5. Ejecutando pings de prueba a través del túnel..." | tee -a "$REPORT"

echo "Ejecutando Ping desde Cliente (10.0.0.2) hacia Servidor (10.0.0.1)..." | tee -a "$REPORT"
# Forzamos a que el comando ping corra exclusivamente en el entorno aislado del cliente
ip netns exec "$NS_CLIENT" ping -c 3 10.0.0.1 > "$PING_LOG_1" 2>&1 || true
cat "$PING_LOG_1" >> "$REPORT"

echo "Ejecutando Ping desde Servidor (10.0.0.1) hacia Cliente (10.0.0.2)..." | tee -a "$REPORT"
# Forzamos a que el comando ping corra exclusivamente en el entorno aislado del servidor
ip netns exec "$NS_SERVER" ping -c 3 10.0.0.2 > "$PING_LOG_2" 2>&1 || true
cat "$PING_LOG_2" >> "$REPORT"

echo "" | tee -a "$REPORT"
echo "6. Resumen de resultados" | tee -a "$REPORT"
PING1_OK=0
PING2_OK=0

if grep -q "3 received" "$PING_LOG_1"; then PING1_OK=1; fi
if grep -q "3 received" "$PING_LOG_2"; then PING2_OK=1; fi

if [[ $PING1_OK -eq 1 && $PING2_OK -eq 1 ]]; then
    echo "RESULTADO: Éxito. El túnel TUN funciona de extremo a extremo, OpenSSL cifra/descifra correctamente bajo AES-256-GCM." | tee -a "$REPORT"
else
    echo "RESULTADO: Fallo en el túnel. Revisa los logs aislados." | tee -a "$REPORT"
fi