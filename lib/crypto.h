#ifndef CRYPTO_H
#define CRYPTO_H

#include<openssl/evp.h>
#include<openssl/rand.h>
#include<string.h>

unsigned char vpn_key[32] = "12345678901234567890123456789012";

#define IV_LEN 12
#define TAG_LEN 16

int vpn_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *dest)
{
EVP_CIPHER_CTX *ctx;
int len;
int ciphertext_len;

unsigned char iv[IV_LEN];
	if(RAND_bytes(iv, IV_LEN) !=1) return -1;
	memcpy(dest, iv, IV_LEN);
	if(!(ctx= EVP_CIPHER_CTX_new())) return -1;

//inicializar cifrado
if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vpn_key, iv)) return 1;
// cifrar los datos
if(1 != EVP_EncryptUpdate(ctx, dest + IV_LEN, &len, plaintext, plaintext_len)) return 1;
}

#endif