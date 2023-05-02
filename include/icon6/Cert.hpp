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

#ifndef ICON6_CERT_HPP
#define ICON6_CERT_HPP

#include <cinttypes>

#include <vector>
#include <string>
#include <memory>

#include <sodium.h>

namespace icon6 {
namespace crypto {
	
	constexpr size_t KEX_PUBLIC_KEY_BYTES = crypto_kx_PUBLICKEYBYTES;
	constexpr size_t KEX_SECRET_KEY_BYTES = crypto_kx_SECRETKEYBYTES;
	
	constexpr size_t SIGN_PUBLIC_KEY_BYTES = crypto_sign_PUBLICKEYBYTES;
	constexpr size_t SIGN_SECRET_KEY_BYTES = crypto_sign_SECRETKEYBYTES;
	constexpr size_t SIGNATURE_BYTES = crypto_sign_BYTES;
	
	class CertReq final {
	public:
		
		
		
		friend class Cert;
		
	private:
		
		std::vector<uint8_t> binaryCertReq;
		
		uint8_t *publicKey;
		uint8_t *parentPublicKey;
		
		uint8_t *selfSignature;
		
		char *url;
		char *name;
	};
	
	class CertKey final {
	public:
		
		static std::shared_ptr<CertKey> GenerateKey();
		static std::shared_ptr<CertKey> LoadKey(std::string fileName,
				const char* password, uint32_t passwordLength);
		
		~CertKey();
		
		void Sign(uint8_t signature[SIGNATURE_BYTES], const void* message,
				uint32_t messageLength);
		bool Verify(const uint8_t signature[SIGNATURE_BYTES],
				const void* message, uint32_t messageLength);
		
		void SaveFile(std::string fileName, const char* password,
				uint32_t passwordLength, uint32_t opslimit = 2,
				uint32_t memlimit = 64*1024*1024) const;
		
	private:
		
		CertKey();
		
	private:
		
		uint8_t privateKey[SIGN_SECRET_KEY_BYTES];
		uint8_t publicKey[SIGN_PUBLIC_KEY_BYTES];
	};
	
	class Cert final {
	public:
		
		~Cert();
		
		inline uint32_t GetCertBytes() const { return binaryCert.size(); }
		void CopyCert(void* dst) const;
		
	private:
		
		Cert(std::string fileName); // load cert from file
		Cert(void* certBinary, uint32_t certBinaryLength); // copy cert from
														   // binary
		Cert(CertReq* certReq, CertKey* parentCertKey, Cert* parentCert);
				// sign certificate with parent cert
		Cert(CertReq* selfSignedCert, CertKey* selfKey);
		
	private:
		
		std::vector<uint8_t> binaryCert;
		
		uint8_t *publicKey;
		uint8_t *parentPublicKey;
		
		uint8_t *selfSignature;
		uint8_t *parentSignature;
		
		char *url;
		char *name;
	};
} // namespace crypto
} // namespace icon6

#endif

