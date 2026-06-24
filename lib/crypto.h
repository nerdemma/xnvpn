#ifndef CRYPTO_H
#define CRYPTO_H

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <string.h>

unsigned char vpn_key[32] = "12345678901234567890123456789012";

#define IV_LEN 12
#define TAG_LEN 16

int vpn_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *dest)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len = 0; 

    unsigned char iv[IV_LEN];
    if(RAND_bytes(iv, IV_LEN) != 1) return -1; 

    memcpy(dest, iv, IV_LEN);

    if(!(ctx = EVP_CIPHER_CTX_new())) return -1;

    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vpn_key, iv))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(1 != EVP_EncryptUpdate(ctx, dest + IV_LEN, &len, plaintext, plaintext_len))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len = len; 

    if(1 != EVP_EncryptFinal_ex(ctx, dest + IV_LEN + ciphertext_len, &len))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;  
    }
    ciphertext_len += len; 

    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, TAG_LEN, dest + IV_LEN + ciphertext_len))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    EVP_CIPHER_CTX_free(ctx);
    return IV_LEN + ciphertext_len + TAG_LEN;
}

int vpn_decrypt(unsigned char *src, int src_len, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;

    if(src_len < (IV_LEN + TAG_LEN)) return -1;
    unsigned char iv[IV_LEN];
    memcpy(iv, src, IV_LEN);
    int ciphertext_len = src_len - IV_LEN - TAG_LEN;
    unsigned char tag[TAG_LEN];
    memcpy(tag, src + IV_LEN + ciphertext_len, TAG_LEN);

    if(!(ctx = EVP_CIPHER_CTX_new())) return -1;

    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, vpn_key, iv)){
        EVP_CIPHER_CTX_free(ctx);
        return -1; 
    }

    if(1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, TAG_LEN, tag))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, src + IV_LEN, ciphertext_len))
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len = len;

    if(EVP_DecryptFinal_ex(ctx, plaintext + len, &len) <= 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        return -1;  
    }
    plaintext_len += len; 
    
    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

#endif