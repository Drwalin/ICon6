/*
 *  This file is part of ICon6.
 *  Copyright (C) 2023 Marek Zalewski aka Drwalin
 *
 *  ICon6 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ICon6 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstring>

#include "../include/icon6/Cert.hpp"

namespace icon6
{
namespace crypto
{

CertKey *CertKey::GenerateKey()
{
	CertKey *key = new CertKey();
	crypto_sign_ed25519_keypair(key->publicKey, key->secretKey);
	return key;
}

CertKey::CertKey() { sodium_mlock(this, sizeof(CertKey)); }

CertKey::~CertKey() { sodium_munlock(this, sizeof(CertKey)); }

void CertKey::Sign(uint8_t signature[SIGNATURE_BYTES], const void *message,
				   uint32_t messageLength)
{
	crypto_sign_ed25519_detached(signature, nullptr, (const uint8_t *)message,
								 messageLength, secretKey);
}

bool CertKey::Verify(const uint8_t signature[SIGNATURE_BYTES],
					 const void *message, uint32_t messageLength)
{
	return !crypto_sign_ed25519_verify_detached(
		signature, (const uint8_t *)message, messageLength, publicKey);
}

void CertKey::CopyPublicKey(void *data)
{
	memcpy(data, publicKey, SIGN_PUBLIC_KEY_BYTES);
}

} // namespace crypto
} // namespace icon6
