/**
 * @file      uaes.c
 * @author    Antonio V. G. Bassi (antoniovitor.gb@gmail.com)
 * @brief     uAES API source code. 
 * @version   0.0
 * @date      2022-12-23 YYYY-MM-DD
 * @note      tab = 2 spaces!
 *
 *  Copyright (C) 2022, Antonio Vitor Grossi Bassi
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "uaes.h"
#include "ops.h"

#define uAES_HEADER_KEY_MSK   (0xFF00)
#define uAES_HEADER_SIZE_MSK  (0x00FF)

#define uAES128_KSCHD_SIZE    (44UL)
#define uAES192_KSCHD_SIZE    (52UL)
#define uAES256_KSCHD_SIZE    (60UL)
#define uAES_MAX_KSCHD_SIZE   (60UL)

#define uAES_HEADER_GET_CRYPTO_MODE(h)      (aes_length_t)((h & uAES_HEADER_KEY_MSK) >> 8 )
#define uAES_HEADER_GET_SIZE(h)             (size_t)((h & uAES_HEADER_SIZE_MSK) >> 0 )
#define uAES_HEADER_PUT_CRYPTO_MODE(h, kt)  h |= (uint16_t)(((uint8_t) kt ) << 8 )
#define uAES_HEADER_PUT_SIZE(h, l)          h |= (uint16_t)(((uint8_t) l ) << 0 )

uint8_t trace_msk   = 0x00;
static uint8_t input_buffer[uAES_MAX_INPUT_SIZE] = {0};
static uint8_t key_buffer[uAES_MAX_KEY_SIZE]     = {0};

static size_t uaes_strnlen(char *str, size_t lim);
static void   uaes_xor_iv(uint8_t *blk, uint8_t *iv);
static void   uaes_foward_cipher(uint8_t *buf, uint32_t *kschd, size_t Nk, size_t Nb, size_t Nr);
static void   uaes_inverse_cipher(uint8_t *buf, uint32_t *kschd, size_t Nk, size_t Nb, size_t Nr);

/**
 * @brief Sets trace mask for debugging.
 * @param msk unsigned 8-bit variable expressing debugging options to be enabled.
 * @note If __uAES_DEBUG__ is not defined this function is not available.
 * @return uint8_t Returns current trace mask.
 */
#ifdef __uAES_DEBUG__
int     debug_line  = 0;
uint8_t uaes_set_trace_msk(unsigned char msk)
{
  trace_msk |= msk;
  return trace_msk;
}
#else
uint8_t uaes_set_trace_msk(unsigned char msk)
{
  return 0;
}
#endif /*__uAES_DEBUG__*/

static size_t uaes_strnlen(char *str, size_t lim)
{
  size_t s = 0;
  if(( NULL != str) && (s < lim))
  {
    for(s = 0; ((s < lim)&&(str[s] != 0x00)); s++);
  }
  return s;
}

/**
 * @brief Performs XOR operation between initialisation vector and data block
 * 
 * @param blk Pointer to data block.
 * @param iv  Pointer to initialisation vector.
 */
static void uaes_xor_iv(uint8_t *blk, uint8_t *iv)
{
  for(size_t c = 0UL; c < 4UL; c++)
  {
    blk[4*c + 0] ^= iv[4*c + 0];
    blk[4*c + 1] ^= iv[4*c + 1];   
    blk[4*c + 2] ^= iv[4*c + 2];
    blk[4*c + 3] ^= iv[4*c + 3];
  }
  return;
}

/**
 * @brief Computes foward cipher encryption on provided buffer.
 * @param data  Pointer to data buffer.
 * @param kschd Pointer to key schedule buffer generated by key expansion algorithm. 
 * @param Nk    Key length in 32-bit words.
 * @param Nb    Block length in 32-bit words.
 * @param Nr    Number of encryption rounds.
 * @return int If successful returns a 0, otherwise -1 will be returned.
 */
static void uaes_foward_cipher(uint8_t *buf, uint32_t *kschd, size_t Nk, size_t Nb, size_t Nr)
{
  uint8_t block[uAES_BLOCK_SIZE] = {0};

  memcpy((void *)block, (void *)buf, uAES_BLOCK_SIZE);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].block = ", block, (size_t)0);
  add_round_key(block, kschd, 0, Nb);
  for(size_t round = 1; round < Nr; round++)
  {
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].start = ", block, round);
    sub_block(block, Nb);
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].s_box = ", block, round);
    shift_rows(block, Nb);
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].sh_row = ", block, round);
    mix_columns(block, Nb);
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].m_col = ", block, round);
    add_round_key(block, kschd, round, Nb);
  }
  sub_block(block, Nb);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].s_box = ", block, Nr);
  shift_rows(block, Nb);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].sh_row = ", block, Nr);
  add_round_key(block, kschd, Nr, Nb);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].end = ", block, Nr);
  memcpy((void *)buf, (void *)block, uAES_BLOCK_SIZE);
  return;
}

/**
 * @brief Computes inverse cipher decryption on provided buffer.
 * @param data  Pointer to ciphertext buffer.
 * @param kschd Pointer to key schedule buffer generated by key expansion algorithm. 
 * @param Nk    Number of 32-bit words in a key.
 * @param Nb    Number of 32-bit words in a block.
 * @param Nr    Number of rounds.
 */
static void uaes_inverse_cipher(uint8_t *buf, uint32_t *kschd, size_t Nk, size_t Nb, size_t Nr)
{
  uint8_t block[uAES_BLOCK_SIZE] = {0};

  memcpy((void *) block, (void *) buf, uAES_BLOCK_SIZE);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_INV, "round[%lu].block = ", block, Nr);
  add_round_key(block, kschd, Nr, Nb);
  for(size_t round = Nr - 1; round > 0; round--)
  {
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_INV, "round[%lu].start = ", block, round);
    inv_shift_rows(block, Nb);
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_INV, "round[%lu].inv_sh_row = ", block, round);
    inv_sub_block(block, Nb);
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_INV, "round[%lu].inv_s_box = ", block, round);
    add_round_key(block, kschd, round, Nb);
    uAES_TRACE_BLOCK(uAES_TRACE_MSK_INV, "round[%lu].add_rkey = ", block, round);
    inv_mix_columns(block, Nb);
  }
  inv_shift_rows(block, Nb);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_INV, "round[%lu].inv_sh_row = ", block, (size_t)0);
  inv_sub_block(block, Nb);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_INV, "round[%lu].inv_s_box = ", block, (size_t)0);
  add_round_key(block, kschd, 0, Nb);
  uAES_TRACE_BLOCK(uAES_TRACE_MSK_FWD, "round[%lu].end = ", block, (size_t)0);
  memcpy((void *)buf, (void *)block, uAES_BLOCK_SIZE);  
  return;
}

/**
 * @brief Performs AES Cipher Block Chaining encryption on given plaintext.
 * 
 * @param plaintext       Pointer to plaintext buffer
 * @param plaintext_size  Plaintext buffer size.
 * @param key             Pointer to key buffer.
 * @param init_vec        16-Byte Initialisation vector.
 * @param aes_mode        Encryption/Decryption mode.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
int uaes_cbc_encryption(uint8_t   *plaintext, 
                        size_t    plaintext_size, 
                        uint8_t   *key, 
                        uint8_t   *init_vec,
                        aes_length_t  aes_mode)
{
  int       err = -1;
  uint32_t  kschd[uAES_MAX_KSCHD_SIZE] = {0};
  size_t Nk, Nb, Nr, idx = 0, offset = plaintext_size;

  if(0 != (plaintext_size & uAES_BLOCK_ALIGN_MASK))
  {
    offset = uAES_ALIGN(plaintext_size, uAES_BLOCK_ALIGN);
  }
  offset >>= 4UL;

  Nk = (uAES128 == aes_mode)?(4UL ):((uAES192 == aes_mode)?(6UL ):((uAES256 == aes_mode)?(8UL ):(0UL)));
  Nr = (uAES128 == aes_mode)?(10UL):((uAES192 == aes_mode)?(12UL):((uAES256 == aes_mode)?(14UL):(0UL)));
  Nb = 4UL;

  if((NULL != key)                           && 
     (NULL != plaintext)                     &&
     (NULL != init_vec)                      && 
     (0 < plaintext_size)                    && 
     (uAES_MAX_INPUT_SIZE >= plaintext_size) && 
     (uAESRGE > aes_mode))
  {
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    uaes_xor_iv(plaintext, init_vec);
    uaes_foward_cipher(&plaintext[uAES_BLOCK_SIZE*idx], kschd, Nk, Nb, Nr);
    idx++;
    while(offset > idx)
    {
      uaes_xor_iv(&plaintext[uAES_BLOCK_SIZE*idx], &plaintext[uAES_BLOCK_SIZE*(idx-1)]);
      uaes_foward_cipher(&plaintext[uAES_BLOCK_SIZE*idx], kschd, Nk, Nb, Nr);
      idx++;
    }
    err = 0;
  }
  return err;
}

/**
 * @brief Performs AES Cipher Block Chaining decryption on given ciphertext 
 * 
 * @param ciphertext      Pointer to ciphertext buffer.
 * @param ciphertext_size Ciphertext buffer size.
 * @param key             Pointer to key buffer.
 * @param init_vec        16-Byte initialisation vector.
 * @param aes_mode        Encryption/Decryption mode.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
int uaes_cbc_decryption(uint8_t   *ciphertext, 
                        size_t    ciphertext_size, 
                        uint8_t   *key, 
                        uint8_t   *init_vec,
                        aes_length_t  aes_mode)
{
  int       err = -1;
  uint32_t  kschd[uAES_MAX_KSCHD_SIZE] = {0};
  size_t Nk, Nb, Nr, idx = 0, offset = ciphertext_size;

  if(0 != (ciphertext_size & uAES_BLOCK_ALIGN_MASK))
  {
    offset = uAES_ALIGN(ciphertext_size, uAES_BLOCK_ALIGN);
  }
  offset >>= 4UL;

  Nk = (uAES128 == aes_mode)?(4UL ):((uAES192 == aes_mode)?(6UL ):((uAES256 == aes_mode)?(8UL ):(0UL)));
  Nr = (uAES128 == aes_mode)?(10UL):((uAES192 == aes_mode)?(12UL):((uAES256 == aes_mode)?(14UL):(0UL)));
  Nb = 4UL;

  if((NULL != key)                            && 
     (NULL != ciphertext)                     &&
     (NULL != init_vec)                       && 
     (0 < ciphertext_size)                    && 
     (uAES_MAX_INPUT_SIZE >= ciphertext_size) && 
     (uAESRGE > aes_mode))
  {
    idx = offset - 1UL;
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    while(idx > 0)
    {
      uaes_inverse_cipher(&ciphertext[uAES_BLOCK_SIZE*idx], kschd, Nk, Nb, Nr);
      uaes_xor_iv(&ciphertext[uAES_BLOCK_SIZE*idx], &ciphertext[uAES_BLOCK_SIZE*(idx-1)]);
      idx--;
    }
    uaes_inverse_cipher(&ciphertext[uAES_BLOCK_SIZE*idx], kschd, Nk, Nb, Nr);
    uaes_xor_iv(&ciphertext[uAES_BLOCK_SIZE*idx], init_vec);
    err = 0;
  }
  return err;
}

/**
 * NOTE: AES-ECB IS NO LONGER CONSIDERED SAFE, USE IT AT YOUR OWN RISK.
 * 
 * @brief Performs AES Electronic Code Book encryption on given plaintext.
 * 
 * @param plaintext       Pointer to plaintext buffer.
 * @param plaintext_size  Size of plaintext buffer.
 * @param key             Pointer to key buffer.
 * @param aes_mode        Encryption/Decryption mode. 
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
int uaes_ecb_encryption(uint8_t   *plaintext, 
                        size_t    plaintext_size, 
                        uint8_t   *key, 
                        aes_length_t  aes_mode)
{
  int err = -1;
  uint32_t kschd[uAES_MAX_KSCHD_SIZE] = {0}; 
  size_t Nk, Nb, Nr, idx = 0, offset = plaintext_size;

  if(0 != (plaintext_size & uAES_BLOCK_ALIGN_MASK))
  {
    offset = uAES_ALIGN(plaintext_size, uAES_BLOCK_ALIGN);
  }
  offset >>= 4UL;

  Nk = (uAES128 == aes_mode)?(4UL ):((uAES192 == aes_mode)?(6UL ):((uAES256 == aes_mode)?(8UL ):(0UL)));
  Nr = (uAES128 == aes_mode)?(10UL):((uAES192 == aes_mode)?(12UL):((uAES256 == aes_mode)?(14UL):(0UL)));
  Nb = 4UL;

  if((NULL != key)                           && 
     (NULL != plaintext)                     && 
     (0 < plaintext_size)                    && 
     (uAES_MAX_INPUT_SIZE >= plaintext_size) && 
     (uAESRGE > aes_mode))
  {
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    while(offset > idx)
    {
      uaes_foward_cipher(&plaintext[uAES_BLOCK_SIZE*idx], kschd, Nk, Nb, Nr);
      idx++;
    }
    err = 0;
  }
  return err;
}

/**
 * NOTE: AES-ECB IS NO LONGER CONSIDERED SAFE, USE IT AT YOUR OWN RISK.
 * 
 * @brief Performs AES-ECB decryption on given plaintext.
 * 
 * @param ciphertext      Pointer to ciphertext buffer.
 * @param ciphertext_size Size of ciphertext buffer.
 * @param key             Pointer to key buffer.
 * @param aes_mode        Encryption/Decryption mode. 
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
int uaes_ecb_decryption(uint8_t   *ciphertext, 
                        size_t    ciphertext_size, 
                        uint8_t   *key, 
                        aes_length_t  aes_mode)
{
  int err = -1;
  uint32_t kschd[uAES_MAX_KSCHD_SIZE] = {0};
  size_t Nk, Nb, Nr, idx = 0, offset = ciphertext_size;

  if(0 != (ciphertext_size & uAES_BLOCK_ALIGN_MASK))
  {
    offset = uAES_ALIGN(ciphertext_size, uAES_BLOCK_ALIGN);
  }
  offset >>= 4UL;

  Nk = (uAES128 == aes_mode)?(4UL ):((uAES192 == aes_mode)?(6UL ):((uAES256 == aes_mode)?(8UL ):(0UL)));
  Nr = (uAES128 == aes_mode)?(10UL):((uAES192 == aes_mode)?(12UL):((uAES256 == aes_mode)?(14UL):(0UL)));
  Nb = 4UL;

  if((NULL != key)                            && 
     (NULL != ciphertext)                     && 
     (0 < ciphertext_size)                    && 
     (uAES_MAX_INPUT_SIZE >= ciphertext_size) && 
     (uAESRGE > aes_mode))
  {
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    while(offset > idx)
    {
      uaes_inverse_cipher(&ciphertext[uAES_BLOCK_SIZE*idx], kschd, Nk, Nb, Nr);
      idx++;
    }
    err = 0;
  }
  return err;
}

/**
 * @brief Computes AES-128 encryption on a single 16 byte plaintext block.
 * 
 * @param plaintext       Pointer to plaintext buffer.
 * @param key             Pointer to key buffer.
 * @param plaintext_size  Plaintext buffer size.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
int uaes128enc(uint8_t *plaintext, uint8_t *key, size_t plaintext_size)
{
  int err = -1;
  const size_t Nk = 4, Nb = 4, Nr = 10;
  uint32_t kschd[uAES128_KSCHD_SIZE] = {0};
  if((NULL != key) && (NULL != plaintext) && (0 < plaintext_size) && (uAES_BLOCK_SIZE >= plaintext_size))
  {
    err = 0;
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    uaes_foward_cipher(plaintext, kschd, Nk, Nb, Nr);
  }
  return err;
}

/**
 * @brief Computes AES-192 encryption on a single 16 byte plaintext block.
 * 
 * @param plaintext       Pointer to plaintext buffer.
 * @param key             Pointer to key buffer.
 * @param plaintext_size  Plaintext buffer size.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
int uaes192enc(uint8_t *plaintext, uint8_t *key, size_t plaintext_size)
{
  int err = -1;
  const size_t Nk = 6, Nb = 4, Nr = 12;
  uint32_t kschd[uAES192_KSCHD_SIZE] = {0};
  if((NULL != key) && (NULL != plaintext) && (0 < plaintext_size) && (uAES_BLOCK_SIZE >= plaintext_size))
  {
    err = 0;
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    uaes_foward_cipher(plaintext, kschd, Nk, Nb, Nr);
  }
  return err;
}

/**
 * @brief Computes AES-256 encryption on a single 16 byte plaintext block.
 * 
 * @param plaintext       Pointer to plaintext buffer
 * @param key             Pointer to key buffer
 * @param plaintext_size  Plaintext buffer size.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
int uaes256enc(uint8_t *plaintext, uint8_t *key, size_t plaintext_size)
{
  int err = -1;
  const size_t Nk = 8, Nb = 4, Nr = 14;
  uint32_t kschd[uAES256_KSCHD_SIZE] = {0};
  if((NULL != key) && (NULL != plaintext) && (0 < plaintext_size) && (uAES_BLOCK_SIZE >= plaintext_size))
  {
    err = 0;
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    uaes_foward_cipher(plaintext, kschd, Nk, Nb, Nr);
  }
  return err;
}

/**
 * @brief Computes AES-128 decryption on a single 16 byte ciphertext block.
 * 
 * @param Ciphertext      Pointer to ciphertext buffer
 * @param key             Pointer to key buffer
 * @param Ciphertext_size Ciphertext buffer size.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
extern int uaes128dec(uint8_t *ciphertext, uint8_t *key, size_t ciphertext_size)
{
  int err = -1;
  const size_t Nk = 4, Nb = 4, Nr = 10;
  uint32_t kschd[uAES128_KSCHD_SIZE] = {0};
  if((NULL != key) && (NULL != ciphertext) && (0 < ciphertext_size) && (uAES_BLOCK_SIZE >= ciphertext_size))
  {
    err = 0;
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    uaes_inverse_cipher(ciphertext, kschd, Nk, Nb, Nr);
  }
  return err;
}

/**
 * @brief Computes AES-192 decryption on a single 16 byte ciphertext block.
 * 
 * @param Ciphertext      Pointer to ciphertext buffer
 * @param key             Pointer to key buffer
 * @param Ciphertext_size Ciphertext buffer size.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
extern int uaes192dec(uint8_t *ciphertext, uint8_t *key, size_t ciphertext_size)
{
  int err = -1;
  const size_t Nk = 6, Nb = 4, Nr = 12;
  uint32_t kschd[uAES192_KSCHD_SIZE] = {0};
  if((NULL != key) && (NULL != ciphertext) && (0 < ciphertext_size) && (uAES_BLOCK_SIZE >= ciphertext_size))
  {
    err = 0;
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    uaes_inverse_cipher(ciphertext, kschd, Nk, Nb, Nr);
  }
  return err;
}

/**
 * @brief Computes AES-256 decryption on a single 16 byte ciphertext block.
 * 
 * @param Ciphertext      Pointer to ciphertext buffer
 * @param key             Pointer to key buffer
 * @param Ciphertext_size Ciphertext buffer size.
 * @return int [ 0] if sucessful.
 *             [-1] on failure. 
 */
extern int uaes256dec(uint8_t *ciphertext, uint8_t *key, size_t ciphertext_size)
{
  int err = -1;
  const size_t Nk = 8, Nb = 4, Nr = 14;
  uint32_t kschd[uAES256_KSCHD_SIZE] = {0};
  if((NULL != key) && (NULL != ciphertext) && (0 < ciphertext_size) && (uAES_BLOCK_SIZE >= ciphertext_size))
  {
    err = 0;
    key_expansion(key, kschd, Nk, (Nb*(Nr+1)));
    uaes_inverse_cipher(ciphertext, kschd, Nk, Nb, Nr);
  }
  return err;
}

