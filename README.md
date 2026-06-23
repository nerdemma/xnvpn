# Custom L3 UDP VPN with OpenSSL (AES-256-GCM)

This project is a fully functional prototype of a self-managed **Layer 3 (L3) VPN**, written in **C/C++** for **Linux (Debian 13)**. It leverages the kernel's virtual `TUN` subsystem to capture traffic, high-performance `UDP` sockets for data transport, and the **OpenSSL** library to provide an encrypted, authenticated channel.

It is particularly well-suited for securing browsing traffic when connecting from untrusted public Wi-Fi networks (such as those in a coffee shop), by transparently tunneling requests to a trusted home server.

---

## 🚀 Features & Technologies

- **Layer 3 Networking:** Uses software-managed virtual `TUN` interfaces (`tun0` / `tun1`) to capture and inject raw IP packets directly at the network layer.
- **UDP Transport:** Implements `SOCK_DGRAM` sockets to avoid the *TCP Meltdown* problem — the performance collapse caused by double TCP encapsulation — while keeping latency low.
- **Authenticated Encryption (AEAD):** Integrates OpenSSL's EVP API with the **AES-256-GCM** algorithm. Each packet carries a unique 12-byte Initialization Vector (IV) and a 16-byte authentication tag (GCM Tag), providing both confidentiality and integrity — making data tampering detectable.
- **Bash Automation:** Network interfaces are configured dynamically via native Linux system calls (`ip addr`, `ip link`), with no manual setup required.

---

## 📊 System Architecture

Data flows from a remote client to the internet through the following cycle: