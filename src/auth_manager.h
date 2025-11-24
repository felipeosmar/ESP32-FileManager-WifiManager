#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include <mbedtls/md.h>
#include <string.h>

class AuthManager {
public:
    /**
     * @brief Gera hash SHA256 de uma string
     * @param input String a ser hasheada (const char*)
     * @param output Buffer para armazenar o hash (mínimo 65 bytes: 64 chars + null terminator)
     * @param outputSize Tamanho do buffer de saída
     * @return true se hash gerado com sucesso, false se buffer insuficiente
     *
     * IMPORTANTE: Esta versão não aloca memória dinamicamente!
     * Elimina 32 alocações/dealocações que a versão anterior tinha.
     */
    static bool hashPassword(const char* input, char* output, size_t outputSize) {
        // Validar parâmetros
        if (input == nullptr || output == nullptr || outputSize < 65) {
            return false;
        }

        byte hash[32];
        mbedtls_md_context_t ctx;
        mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
        mbedtls_md_starts(&ctx);
        mbedtls_md_update(&ctx, (const unsigned char*)input, strlen(input));
        mbedtls_md_finish(&ctx, hash);
        mbedtls_md_free(&ctx);

        // Converter para hexadecimal DIRETAMENTE no buffer (SEM alocações!)
        for (int i = 0; i < 32; i++) {
            snprintf(&output[i * 2], 3, "%02x", hash[i]);
        }
        output[64] = '\0';  // Garantir null terminator

        return true;
    }

    /**
     * @brief Verifica se uma senha corresponde ao hash
     * @param password Senha em texto plano (const char*)
     * @param hash Hash SHA256 para comparar (64 caracteres hexadecimais)
     * @return true se a senha corresponde ao hash
     */
    static bool verifyPassword(const char* password, const char* hash) {
        if (password == nullptr || hash == nullptr) {
            return false;
        }

        char passwordHash[65];
        if (!hashPassword(password, passwordHash, sizeof(passwordHash))) {
            return false;
        }

        // Comparação case-insensitive
        return strcasecmp(passwordHash, hash) == 0;
    }

    /**
     * @brief Valida força da senha
     * @param password Senha a ser validada (const char*)
     * @return true se a senha atende aos requisitos mínimos
     *
     * Requisitos:
     * - Mínimo 8 caracteres
     * - Pelo menos 1 letra maiúscula
     * - Pelo menos 1 letra minúscula
     * - Pelo menos 1 número
     */
    static bool isPasswordStrong(const char* password) {
        if (password == nullptr) {
            return false;
        }

        size_t len = strlen(password);
        if (len < 8) {
            return false;
        }

        bool hasUpper = false;
        bool hasLower = false;
        bool hasDigit = false;

        for (size_t i = 0; i < len; i++) {
            char c = password[i];
            if (isupper(c)) hasUpper = true;
            if (islower(c)) hasLower = true;
            if (isdigit(c)) hasDigit = true;
        }

        return hasUpper && hasLower && hasDigit;
    }

    /**
     * @brief Retorna mensagem de erro de validação de senha
     * @param password Senha a ser validada (const char*)
     * @param errorBuffer Buffer para armazenar mensagem de erro (mínimo 64 bytes)
     * @param bufferSize Tamanho do buffer de erro
     * @return true se senha válida (errorBuffer estará vazio), false se inválida
     */
    static bool getPasswordValidationError(const char* password, char* errorBuffer, size_t bufferSize) {
        if (password == nullptr || errorBuffer == nullptr || bufferSize == 0) {
            return false;
        }

        // Limpar buffer
        errorBuffer[0] = '\0';

        size_t len = strlen(password);
        if (len < 8) {
            strlcpy(errorBuffer, "Senha deve ter no minimo 8 caracteres", bufferSize);
            return false;
        }

        bool hasUpper = false;
        bool hasLower = false;
        bool hasDigit = false;

        for (size_t i = 0; i < len; i++) {
            char c = password[i];
            if (isupper(c)) hasUpper = true;
            if (islower(c)) hasLower = true;
            if (isdigit(c)) hasDigit = true;
        }

        if (!hasUpper) {
            strlcpy(errorBuffer, "Senha deve conter pelo menos 1 letra maiuscula", bufferSize);
            return false;
        }

        if (!hasLower) {
            strlcpy(errorBuffer, "Senha deve conter pelo menos 1 letra minuscula", bufferSize);
            return false;
        }

        if (!hasDigit) {
            strlcpy(errorBuffer, "Senha deve conter pelo menos 1 numero", bufferSize);
            return false;
        }

        return true;  // Senha válida
    }
};

#endif // AUTH_MANAGER_H
