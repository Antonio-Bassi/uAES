/**
 * @file      uaes.h
 * @author    Antonio V. G. Bassi (antoniovitor.gb@gmail.com)
 * @brief     uAES API references and global data-types. 
 * @version   0.0
 * @date      2022-12-23 YYYY-MM-DD
 * @copyright Copyright (c) 2022
 * @note      tab = 2 spaces! 
 */

#ifndef UAES_H
#define UAES_H

#define uAES_MAX_INPUT_SIZE   (64UL)
#define uAES_MAX_KEY_SIZE     (32UL)
#define uAES_BLOCK_SIZE       (16UL)

typedef enum crypto
{
  uAES128 = 0,  // 128 bit-sized key.
  uAES192 = 1,  // 192 bit-sized key.
  uAES256 = 2,  // 256 bit-sized key.
  uAESRGE = 3   // Range of length options
}crypto_t;

/* Debug */
extern uint8_t   uaes_set_trace_msk(uint8_t msk);

/* Encryption API*/

/** 
 * NOTE: AES-ECB IS NO LONGER CONSIDERED SAFE, USE IT AT YOUR OWN RISK. 
 */
extern int uaes_ecb_encryption( uint8_t   *plaintext, 
                                size_t    plaintext_size, 
                                uint8_t   *key, 
                                crypto_t  crypto_mode);

extern int uaes_cbc_encryption( uint8_t   *plaintext, 
                                size_t    plaintext_size, 
                                uint8_t   *key, 
                                uint8_t   *init_vec,
                                crypto_t  aes_mode);

extern int uaes128enc(uint8_t *plaintext, uint8_t *key, size_t plaintext_size);
extern int uaes192enc(uint8_t *plaintext, uint8_t *key, size_t plaintext_size);
extern int uaes256enc(uint8_t *plaintext, uint8_t *key, size_t plaintext_size);

/* Decryption API*/

/** 
 * NOTE: AES-ECB IS NO LONGER CONSIDERED SAFE, USE IT AT YOUR OWN RISK. 
 */
extern int uaes_ecb_decryption( uint8_t   *ciphertext, 
                                size_t    ciphertext_size, 
                                uint8_t   *key, 
                                crypto_t  crypto_mode );

extern int uaes_cbc_decryption( uint8_t   *plaintext, 
                                size_t    plaintext_size, 
                                uint8_t   *key, 
                                uint8_t   *init_vec,
                                crypto_t  aes_mode );


extern int uaes128dec(uint8_t *ciphertext, uint8_t *key, size_t ciphertext_size);
extern int uaes192dec(uint8_t *ciphertext, uint8_t *key, size_t ciphertext_size);
extern int uaes256dec(uint8_t *ciphertext, uint8_t *key, size_t ciphertext_size);

#endif /*UAES_H*/