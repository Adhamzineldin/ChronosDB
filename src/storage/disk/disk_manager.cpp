#include "storage/disk/disk_manager.h"
#include "common/encryption.h"
#include <stdexcept>
#include <cstring>
#include <filesystem>
#include <cstdio>
#include <iostream>
#include <vector> // FIX: Missing header

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace francodb
{
    constexpr char FRAME_FILE_MAGIC[] = "FRANCO_DATABASE_MADE_BY_MAAYN";
    constexpr size_t MAGIC_LEN = sizeof(FRAME_FILE_MAGIC) - 1;
    constexpr char META_FILE_MAGIC[] = "FRANCO_META";
    constexpr size_t META_MAGIC_LEN = sizeof(META_FILE_MAGIC) - 1;

    uint32_t CalculateChecksum(const char* data)
    {
        uint32_t sum = 0;
        for (size_t i = sizeof(uint32_t); i < PAGE_SIZE; i++)
        {
            sum += static_cast<uint8_t>(data[i]);
        }
        return sum;
    }

    void UpdatePageChecksum(char* page_data, uint32_t page_id)
    {
        if (page_id <= 2) return;
        std::memset(page_data, 0, sizeof(uint32_t));
        uint32_t checksum = CalculateChecksum(page_data);
        std::memcpy(page_data, &checksum, sizeof(uint32_t));
    }

    DiskManager::DiskManager(const std::string& db_file)
    {
        // Init members
        encryption_enabled_ = false;

        std::filesystem::path path(db_file);
        if (path.extension() != ".francodb") file_name_ = db_file + ".francodb";
        else file_name_ = db_file;

        meta_file_name_ = file_name_ + ".meta";

#ifdef _WIN32
        db_io_handle_ = CreateFileA(file_name_.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (db_io_handle_ == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open database: " + file_name_);
#else
        db_io_fd_ = open(file_name_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (db_io_fd_ == -1) throw std::runtime_error("Failed to open database: " + file_name_);
#endif

        if (GetFileSize(file_name_) == 0)
        {
            char page_buffer[PAGE_SIZE];
            std::memset(page_buffer, 0, PAGE_SIZE);
            std::memcpy(page_buffer, FRAME_FILE_MAGIC, MAGIC_LEN);
            WritePage(0, page_buffer);

            std::memset(page_buffer, 0, PAGE_SIZE);
            WritePage(1, page_buffer);

            std::memset(page_buffer, 0, PAGE_SIZE);
            page_buffer[0] = 0x07;
            WritePage(2, page_buffer);
            FlushLog();
            std::cout << "[INFO] Initialized DB: " << file_name_ << std::endl;
        }
        else
        {
            char magic_page[PAGE_SIZE];
            ReadPage(0, magic_page);
            if (std::memcmp(magic_page, FRAME_FILE_MAGIC, MAGIC_LEN) != 0)
            {
                throw std::runtime_error("CORRUPTION: Invalid Header");
            }
        }
    }

    DiskManager::~DiskManager() { ShutDown(); }

    // --- [FIX] THIS IS THE MISSING LOGIC FOR ENCRYPTION ---
    void DiskManager::SetEncryptionKey(const std::string& key)
    {
        if (!key.empty())
        {
            encryption_key_ = key;
            encryption_enabled_ = true;
        }
        else
        {
            encryption_enabled_ = false;
        }
    }

    void DiskManager::ShutDown()
    {
#ifdef _WIN32
        if (db_io_handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(db_io_handle_);
            db_io_handle_ = INVALID_HANDLE_VALUE;
        }
#else
        if (db_io_fd_ != -1)
        {
            close(db_io_fd_);
            db_io_fd_ = -1;
        }
#endif
    }

    void DiskManager::ReadPage(uint32_t page_id, char* page_data)
    {
        std::lock_guard<std::mutex> guard(io_mutex_);
        uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;
        std::memset(page_data, 0, PAGE_SIZE);
        uint32_t bytes_read = 0;

#ifdef _WIN32
        OVERLAPPED overlapped;
        std::memset(&overlapped, 0, sizeof(OVERLAPPED));
        overlapped.Offset = static_cast<DWORD>(offset);
        overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD win_bytes;
        ReadFile(db_io_handle_, page_data, PAGE_SIZE, &win_bytes, &overlapped);
        bytes_read = win_bytes;
#else
        bytes_read = pread(db_io_fd_, page_data, PAGE_SIZE, offset);
#endif

        if (bytes_read < PAGE_SIZE) return;

        // Decrypt
        if (encryption_enabled_ && !encryption_key_.empty() && page_id > 0)
        {
            Encryption::DecryptXOR(encryption_key_, page_data, PAGE_SIZE);
        }

        // Verify Checksum
        if (page_id > 2)
        {
            uint32_t stored_checksum;
            std::memcpy(&stored_checksum, page_data, sizeof(uint32_t));
            uint32_t calculated = CalculateChecksum(page_data);
            if (stored_checksum != 0 && stored_checksum != calculated)
            {
                std::cerr << "[CRITICAL] Checksum Mismatch Page " << page_id << std::endl;
            }
        }
    }

    void DiskManager::WritePage(uint32_t page_id, const char* page_data)
    {
        std::lock_guard<std::mutex> guard(io_mutex_);
        char processed_data[PAGE_SIZE];
        std::memcpy(processed_data, page_data, PAGE_SIZE);

        if (page_id > 2)
        {
            std::memset(processed_data, 0, sizeof(uint32_t));
            uint32_t checksum = CalculateChecksum(processed_data);
            std::memcpy(processed_data, &checksum, sizeof(uint32_t));
        }

        // Encrypt
        if (encryption_enabled_ && !encryption_key_.empty() && page_id > 0)
        {
            Encryption::EncryptXOR(encryption_key_, processed_data, PAGE_SIZE);
        }

        uint64_t offset = static_cast<uint64_t>(page_id) * PAGE_SIZE;

#ifdef _WIN32
        OVERLAPPED overlapped;
        std::memset(&overlapped, 0, sizeof(OVERLAPPED));
        overlapped.Offset = static_cast<DWORD>(offset);
        overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
        DWORD written;
        if (!WriteFile(db_io_handle_, processed_data, PAGE_SIZE, &written, &overlapped))
        {
            throw std::runtime_error("Disk Write Failure");
        }
#else
        pwrite(db_io_fd_, processed_data, PAGE_SIZE, offset);
#endif
    }

    void DiskManager::FlushLog()
    {
        std::lock_guard<std::mutex> guard(io_mutex_);
#ifdef _WIN32
        FlushFileBuffers(db_io_handle_);
#else
        fsync(db_io_fd_);
#endif
    }

    int DiskManager::GetFileSize(const std::string& file_name)
    {
        struct stat stat_buf;
        return (stat(file_name.c_str(), &stat_buf) == 0) ? static_cast<int>(stat_buf.st_size) : 0;
    }

    int DiskManager::GetNumPages() { return GetFileSize(file_name_) / PAGE_SIZE; }

    void DiskManager::WriteMetadata(const std::string& data)
    {
        std::lock_guard<std::mutex> guard(io_mutex_);
        std::string temp_meta = meta_file_name_ + ".tmp";
        std::string data_to_write = data;

        if (encryption_enabled_ && !encryption_key_.empty())
        {
            for (size_t i = 0; i < data_to_write.size(); i++)
            {
                data_to_write[i] ^= encryption_key_[i % encryption_key_.size()];
            }
        }

        std::ofstream out(temp_meta, std::ios::binary | std::ios::trunc);
        if (out)
        {
            out.write(META_FILE_MAGIC, META_MAGIC_LEN);
            size_t size = data_to_write.size();
            out.write(reinterpret_cast<const char*>(&size), sizeof(size));
            out.write(data_to_write.c_str(), size);
            out.close();
            std::filesystem::rename(temp_meta, meta_file_name_);
        }
    }

    bool DiskManager::ReadMetadata(std::string& data)
    {
        std::lock_guard<std::mutex> guard(io_mutex_);
        if (!std::filesystem::exists(meta_file_name_)) return false;

        std::ifstream in(meta_file_name_, std::ios::binary);
        if (!in) return false;

        char magic[16] = {0};
        in.read(magic, META_MAGIC_LEN);
        if (std::memcmp(magic, META_FILE_MAGIC, META_MAGIC_LEN) != 0) return false;

        size_t size = 0;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (size > 1024 * 1024 * 50) return false;

        std::vector<char> buffer(size);
        in.read(buffer.data(), size);

        if (encryption_enabled_ && !encryption_key_.empty())
        {
            for (size_t i = 0; i < size; i++)
            {
                buffer[i] ^= encryption_key_[i % encryption_key_.size()];
            }
        }
        data.assign(buffer.data(), size);
        return true;
    }
}
