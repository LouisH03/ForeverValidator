#define _FILE_OFFSET_BITS 64

#include "platform/native/native_system_file_operations.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include "engine/core/fast_string.h"
#include "engine/resources/system_file_operations.h"

namespace {

#ifdef O_CLOEXEC
constexpr int NativeCloseOnExec = O_CLOEXEC;
#else
constexpr int NativeCloseOnExec = 0;
#endif

#ifdef O_DIRECTORY
constexpr int NativeDirectory = O_DIRECTORY;
#else
constexpr int NativeDirectory = 0;
#endif

#ifdef _WIN32
using NativeMode = int;
constexpr int NativeReadFlags = _O_RDONLY | _O_BINARY;
constexpr int NativeWriteFlags = _O_WRONLY | _O_CREAT | _O_BINARY;
constexpr int NativeExclusiveFlags =
        _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY;
#else
using NativeMode = mode_t;
constexpr int NativeReadFlags = O_RDONLY | NativeCloseOnExec;
constexpr int NativeWriteFlags = O_WRONLY | O_CREAT | NativeCloseOnExec;
constexpr int NativeExclusiveFlags =
        O_WRONLY | O_CREAT | O_EXCL | NativeCloseOnExec;
#endif

enum class NativeOpenResult {
    Opened,
    NotFound,
    AlreadyExists,
    Failed,
};

class NativeFile {
public:
    NativeFile() = default;

    ~NativeFile() {
        if (descriptor >= 0) {
#ifdef _WIN32
            ::_close(descriptor);
#else
            ::close(descriptor);
#endif
        }
    }

    NativeFile(const NativeFile &) = delete;
    NativeFile &operator=(const NativeFile &) = delete;

    NativeOpenResult OpenForReading(const std::filesystem::path &path) {
        return Open(path, NativeReadFlags, 0);
    }

    NativeOpenResult OpenForWriting(
            const std::filesystem::path &path,
            bool truncate) {
        int flags = NativeWriteFlags;
        if (truncate) {
#ifdef _WIN32
            flags |= _O_TRUNC;
#else
            flags |= O_TRUNC;
#endif
        }
        return Open(path, flags, 0666);
    }

    NativeOpenResult CreateExclusive(const std::filesystem::path &path) {
        return Open(
                path,
                NativeExclusiveFlags,
                0666);
    }

    NativeOpenResult OpenDirectory(const std::filesystem::path &path) {
#ifdef _WIN32
        (void)path;
        return NativeOpenResult::Failed;
#else
        return Open(
                path,
                O_RDONLY | NativeDirectory | NativeCloseOnExec,
                0);
#endif
    }

    std::optional<std::size_t> ReadSome(
            void *destination,
            std::size_t byteCount) {
        if (byteCount == 0u) {
            return 0u;
        }
        if (descriptor < 0 || destination == nullptr) {
            healthy = false;
            return std::nullopt;
        }
        const std::size_t transferCount = std::min<std::size_t>(
                byteCount,
#ifdef _WIN32
                std::numeric_limits<unsigned int>::max());
        int result = ::_read(
                descriptor,
                destination,
                static_cast<unsigned int>(transferCount));
#else
                static_cast<std::size_t>(
                        std::numeric_limits<ssize_t>::max()));
        ssize_t result;
        do {
            result = ::read(descriptor, destination, transferCount);
        } while (result < 0 && errno == EINTR);
#endif
        if (result < 0) {
            healthy = false;
            return std::nullopt;
        }
        return static_cast<std::size_t>(result);
    }

    std::optional<std::size_t> WriteSome(
            const void *source,
            std::size_t byteCount) {
        if (byteCount == 0u) {
            return 0u;
        }
        if (descriptor < 0 || source == nullptr) {
            healthy = false;
            return std::nullopt;
        }
        const std::size_t transferCount = std::min<std::size_t>(
                byteCount,
#ifdef _WIN32
                std::numeric_limits<unsigned int>::max());
        int result = ::_write(
                descriptor,
                source,
                static_cast<unsigned int>(transferCount));
#else
                static_cast<std::size_t>(
                        std::numeric_limits<ssize_t>::max()));
        ssize_t result;
        do {
            result = ::write(descriptor, source, transferCount);
        } while (result < 0 && errno == EINTR);
#endif
        if (result < 0) {
            healthy = false;
            return std::nullopt;
        }
        return static_cast<std::size_t>(result);
    }

    std::optional<unsigned long> Seek(unsigned long offset) {
        if (offset > ClassicBufferMaximumExtent) {
            healthy = false;
            return std::nullopt;
        }
        const bool seekToEnd = offset == ClassicBufferEndOffset;
        const auto distance = seekToEnd ? 0 : offset;
#ifdef _WIN32
        const __int64 result = ::_lseeki64(
                descriptor,
                static_cast<__int64>(distance),
                seekToEnd ? SEEK_END : SEEK_SET);
#else
        off_t result;
        do {
            result = ::lseek(
                    descriptor,
                    static_cast<off_t>(distance),
                    seekToEnd ? SEEK_END : SEEK_SET);
        } while (result < 0 && errno == EINTR);
#endif
        if (result < 0 ||
            static_cast<unsigned long long>(result) >
                    ClassicBufferMaximumExtent) {
            healthy = false;
            return std::nullopt;
        }
        return static_cast<unsigned long>(result);
    }

    std::optional<unsigned long long> Size() {
        if (descriptor < 0) {
            healthy = false;
            return std::nullopt;
        }
#ifdef _WIN32
        struct _stati64 status {};
        const int result = ::_fstati64(descriptor, &status);
#else
        struct stat status {};
        int result;
        do {
            result = ::fstat(descriptor, &status);
        } while (result != 0 && errno == EINTR);
#endif
        if (result != 0 || status.st_size < 0) {
            healthy = false;
            return std::nullopt;
        }
        return static_cast<unsigned long long>(status.st_size);
    }

    bool Sync() {
        if (descriptor < 0) {
            healthy = false;
            return false;
        }
        int result;
#ifdef _WIN32
        result = ::_commit(descriptor);
#else
        do {
            result = ::fsync(descriptor);
        } while (result != 0 && errno == EINTR);
#endif
        if (result != 0) {
            healthy = false;
            return false;
        }
        return true;
    }

    bool Close() {
        if (descriptor < 0) {
            return healthy;
        }
        const bool wasHealthy = healthy;
        const int closingDescriptor = descriptor;
        descriptor = -1;
#ifdef _WIN32
        const int closeResult = ::_close(closingDescriptor);
#else
        const int closeResult = ::close(closingDescriptor);
#endif
        if (closeResult != 0) {
            healthy = false;
            return false;
        }
        return wasHealthy;
    }

    bool IsOpen() const {
        return descriptor >= 0;
    }

    bool IsHealthy() const {
        return healthy;
    }

private:
    NativeOpenResult Open(
            const std::filesystem::path &path,
            int flags,
            NativeMode permissions) {
        if (descriptor >= 0) {
            healthy = false;
            return NativeOpenResult::Failed;
        }
        healthy = true;
        int openedDescriptor;
#ifdef _WIN32
        openedDescriptor = ::_wopen(path.c_str(), flags, permissions);
#else
        do {
            openedDescriptor = ::open(path.c_str(), flags, permissions);
        } while (openedDescriptor < 0 && errno == EINTR);
#endif
        if (openedDescriptor >= 0) {
            descriptor = openedDescriptor;
            return NativeOpenResult::Opened;
        }
        healthy = false;
        if (errno == ENOENT) {
            return NativeOpenResult::NotFound;
        }
        if (errno == EEXIST) {
            return NativeOpenResult::AlreadyExists;
        }
        return NativeOpenResult::Failed;
    }

    int descriptor = -1;
    bool healthy = true;
};

std::atomic<unsigned long long> NextTemporaryFileId{0u};

bool CreateTemporaryFile(
        const std::filesystem::path &target,
        NativeFile &file,
        std::filesystem::path &temporaryPath) {
    const std::filesystem::path directory = target.parent_path();
    const std::string baseName = target.filename().string();
    const std::string processId = std::to_string(
            static_cast<unsigned long>(
#ifdef _WIN32
                    ::_getpid()
#else
                    ::getpid()
#endif
                    ));
    for (unsigned int attempt = 0u; attempt < 4096u; ++attempt) {
        const unsigned long long id = NextTemporaryFileId.fetch_add(1u);
        const std::filesystem::path candidate = directory /
                (baseName + ".Fbx." + processId + "." +
                 std::to_string(id) + ".tmp");
        const NativeOpenResult result = file.CreateExclusive(candidate);
        if (result == NativeOpenResult::Opened) {
            temporaryPath = candidate;
            return true;
        }
        if (result != NativeOpenResult::AlreadyExists) {
            return false;
        }
    }
    return false;
}

bool EnsureWritableTarget(const std::filesystem::path &target) {
    NativeFile targetFile;
    if (targetFile.OpenForWriting(target, false) !=
        NativeOpenResult::Opened) {
        return false;
    }
    return targetFile.Close();
}

bool RemoveFile(const std::filesystem::path &path) {
#ifdef _WIN32
    std::error_code error;
    const bool removed = std::filesystem::remove(path, error);
    return removed || (!error && !std::filesystem::exists(path, error));
#else
    int result;
    do {
        result = ::unlink(path.c_str());
    } while (result != 0 && errno == EINTR);
    return result == 0 || errno == ENOENT;
#endif
}

bool RenameReplacing(
        const std::filesystem::path &source,
        const std::filesystem::path &target) {
#ifdef _WIN32
    std::error_code error;
    (void)std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(source, target, error);
    return !error;
#else
    int result;
    do {
        result = ::rename(source.c_str(), target.c_str());
    } while (result != 0 && errno == EINTR);
    return result == 0;
#endif
}

bool SyncParentDirectory(const std::filesystem::path &target) {
#ifdef _WIN32
    (void)target;
    return true;
#else
    const std::filesystem::path directory = target.parent_path().empty()
            ? std::filesystem::path(".")
            : target.parent_path();
    NativeFile directoryHandle;
    if (directoryHandle.OpenDirectory(directory) !=
        NativeOpenResult::Opened) {
        return false;
    }
    const bool synced = directoryHandle.Sync();
    return directoryHandle.Close() && synced;
#endif
}

void AppendFilesystemUtf8(std::string &text, std::uint32_t codePoint) {
    if (codePoint < 0x80u) {
        text.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800u) {
        text.push_back(static_cast<char>(0xc0u | (codePoint >> 6u)));
        text.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
    } else if (codePoint < 0x10000u) {
        text.push_back(static_cast<char>(0xe0u | (codePoint >> 12u)));
        text.push_back(static_cast<char>(
                0x80u | ((codePoint >> 6u) & 0x3fu)));
        text.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
    } else {
        text.push_back(static_cast<char>(0xf0u | (codePoint >> 18u)));
        text.push_back(static_cast<char>(
                0x80u | ((codePoint >> 12u) & 0x3fu)));
        text.push_back(static_cast<char>(
                0x80u | ((codePoint >> 6u) & 0x3fu)));
        text.push_back(static_cast<char>(0x80u | (codePoint & 0x3fu)));
    }
}

std::filesystem::path FromEngineName(const CFastStringInt &name) {
    std::string nativeName;
    nativeName.reserve(name.Length());
    const std::u16string_view units = name.View();
    for (std::size_t index = 0u; index < units.size(); ++index) {
        const char16_t codeUnit = units[index];
        if (codeUnit == 0u) {
            break;
        }
        if (codeUnit == 0xfeffu) {
            continue;
        }
        std::uint32_t codePoint = codeUnit;
        if (codeUnit >= 0xd800u && codeUnit <= 0xdbffu) {
            if (index + 1u < units.size() &&
                units[index + 1u] >= 0xdc00u &&
                units[index + 1u] <= 0xdfffu) {
                codePoint = 0x10000u +
                        ((static_cast<std::uint32_t>(codeUnit) - 0xd800u)
                         << 10u) +
                        (static_cast<std::uint32_t>(units[++index]) -
                         0xdc00u);
            } else {
                codePoint = 0xfffdu;
            }
        } else if (codeUnit >= 0xdc00u && codeUnit <= 0xdfffu) {
            codePoint = 0xfffdu;
        }
        AppendFilesystemUtf8(nativeName, codePoint);
    }
#ifndef _WIN32
    if (std::filesystem::path::preferred_separator != '\\') {
        std::replace(
                nativeName.begin(),
                nativeName.end(),
                '\\',
                std::filesystem::path::preferred_separator);
    }
#endif
    return std::filesystem::u8path(nativeName);
}

bool HasWritePermission(std::filesystem::perms permissions) {
    constexpr std::filesystem::perms writePermissions =
            std::filesystem::perms::owner_write |
            std::filesystem::perms::group_write |
            std::filesystem::perms::others_write;
    return (permissions & writePermissions) != std::filesystem::perms::none;
}

class NativeSystemFileHandle final : public tmnf::platform::SystemFileHandle {
public:
    ~NativeSystemFileHandle() override {
        if (file.IsOpen()) {
            Close();
        }
        RemoveTemporaryFile();
    }

    void Flush() override {
        if (mode == CClassicBuffer::EMode::Write && file.IsOpen()) {
            file.Sync();
        }
    }

    unsigned long Write(
            const void *source,
            unsigned long byteCount) override {
        if (mode != CClassicBuffer::EMode::Write || !file.IsOpen() ||
            (source == nullptr && byteCount != 0u)) {
            return 0u;
        }
        const std::optional<std::size_t> written =
                file.WriteSome(source, byteCount);
        if (!written.has_value()) {
            return 0u;
        }
        const unsigned long writtenByteCount =
                static_cast<unsigned long>(*written);
        currentOffset += writtenByteCount;
        return writtenByteCount;
    }

    int SetOffset(unsigned long offset) override {
        if (!file.IsOpen() || !mode.has_value()) {
            return 0;
        }
        const std::optional<unsigned long> position = file.Seek(offset);
        if (!position.has_value()) {
            return 0;
        }
        currentOffset = *position;
        return 1;
    }

    unsigned long GetLength() const override {
        if (!file.IsOpen()) {
            return 0u;
        }
        const std::optional<unsigned long long> size =
                const_cast<NativeFile &>(file).Size();
        if (!size.has_value()) {
            return 0u;
        }
        return static_cast<unsigned long>(std::min<unsigned long long>(
                *size, ClassicBufferMaximumExtent));
    }

    unsigned long Read(
            void *destination,
            unsigned long byteCount) override {
        if (mode != CClassicBuffer::EMode::Read || !file.IsOpen() ||
            (destination == nullptr && byteCount != 0u)) {
            return 0u;
        }
        const std::optional<std::size_t> read =
                file.ReadSome(destination, byteCount);
        const unsigned long readByteCount = read.has_value()
                ? static_cast<unsigned long>(*read)
                : 0u;
        currentOffset += readByteCount;
        return readByteCount;
    }

    int Open(
            const CFastStringInt &filename,
            CClassicBuffer::EMode openMode,
            int secured,
            int truncate,
            int asynchronous,
            int shareRead) override {
        (void)shareRead;
        if (file.IsOpen() && Close() == 0) {
            return 0;
        }
        currentOffset = 0u;
        openedPath.clear();
        atomicTarget.reset();
        mode = openMode;
        if ((openMode != CClassicBuffer::EMode::Read &&
             openMode != CClassicBuffer::EMode::Write) ||
            asynchronous != 0) {
            mode.reset();
            return 0;
        }

        const std::filesystem::path target = FromEngineName(filename);
        if (openMode == CClassicBuffer::EMode::Read) {
            secured = 0;
            openedPath = target;
            if (file.OpenForReading(target) != NativeOpenResult::Opened) {
                AbortOpen();
                return 0;
            }
            return 1;
        }

        if (secured != 0) {
            if (!EnsureWritableTarget(target) ||
                !CreateTemporaryFile(target, file, openedPath)) {
                AbortOpen();
                return 0;
            }
            atomicTarget = target;
            if (truncate == 0 && SetOffset(ClassicBufferEndOffset) == 0) {
                AbortOpen();
                return 0;
            }
            return 1;
        }

        openedPath = target;
        if (file.OpenForWriting(target, truncate != 0) !=
            NativeOpenResult::Opened) {
            AbortOpen();
            return 0;
        }
        if (truncate == 0 && SetOffset(ClassicBufferEndOffset) == 0) {
            AbortOpen();
            return 0;
        }
        return 1;
    }

    int Close() override {
        const bool storingSecured = atomicTarget.has_value();
        bool success = true;
        if (file.IsOpen()) {
            if (storingSecured && mode == CClassicBuffer::EMode::Write) {
                success = file.IsHealthy() && file.Sync();
            }
            if (!file.Close()) {
                success = false;
            }
        } else if (storingSecured) {
            success = false;
        }

        mode.reset();
        currentOffset = 0u;
        if (storingSecured) {
            const std::filesystem::path target = *atomicTarget;
            if (success && RenameReplacing(openedPath, target)) {
                atomicTarget.reset();
                if (!SyncParentDirectory(target)) {
                    success = false;
                }
            } else {
                success = false;
            }
            RemoveTemporaryFile();
        }
        openedPath.clear();
        return success ? 1 : 0;
    }

    int IsStoringSecured() const override {
        return atomicTarget.has_value() ? 1 : 0;
    }

    unsigned long GetOffset() const override {
        return currentOffset;
    }

    bool NeedsClose() const override {
        return file.IsOpen() || atomicTarget.has_value();
    }

private:
    void RemoveTemporaryFile() {
        if (!atomicTarget.has_value() || openedPath.empty()) {
            return;
        }
        RemoveFile(openedPath);
        atomicTarget.reset();
    }

    void AbortOpen() {
        if (file.IsOpen()) {
            file.Close();
        }
        RemoveTemporaryFile();
        openedPath.clear();
        mode.reset();
        currentOffset = 0u;
    }

    NativeFile file;
    std::filesystem::path openedPath;
    std::optional<std::filesystem::path> atomicTarget;
    std::optional<CClassicBuffer::EMode> mode;
    unsigned long currentOffset = 0u;
};

class NativeSystemFileOperations final
        : public tmnf::platform::SystemFileOperations {
public:
    std::unique_ptr<tmnf::platform::SystemFileHandle> CreateHandle()
            const override {
        return std::make_unique<NativeSystemFileHandle>();
    }

    tmnf::platform::SystemFileMetadata ReadMetadata(
            const CFastStringInt &fullName) const override {
        const std::filesystem::path path = FromEngineName(fullName);
        std::error_code error;
        const std::filesystem::file_status status =
                std::filesystem::status(path, error);
        if (error || !std::filesystem::is_regular_file(status)) {
            return {};
        }
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if (error) {
            return {};
        }
        return tmnf::platform::SystemFileMetadata{
            true,
            !HasWritePermission(status.permissions()),
            static_cast<std::uint64_t>(size),
        };
    }
};

NativeSystemFileOperations NativeOperations;

}  // namespace

namespace tmnf::platform {

void InstallNativeSystemFileOperations() noexcept {
    SetSystemFileOperations(&NativeOperations);
}

}  // namespace tmnf::platform
