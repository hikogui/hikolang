
#include "file_ifstream.hpp"
#include <cassert>

namespace hk {

file_ifstream::file_ifstream(hk::path_id path_id)
    : file(path_id)
{
}

[[nodiscard]] std::size_t file_ifstream::read(std::size_t position, std::span<char> buffer)
{
    if (buffer.size() == 0) {
        return 0; // No data to read
    }

    auto lock = std::unique_lock(_mutex);

    lock = _open(std::move(lock));

    if (_file_stream.eof()) {
        // If the end of the file has been reached, we return 0.
        return 0;
    }

    _file_stream.seekg(position);
    if (_file_stream.bad()) {
        // If the file stream is in a bad state, we close it and return 0.
        // This can happen if the file was not opened successfully or if it
        // was closed while we were trying to read from it.
        lock = _close(std::move(lock));
        auto const &path = get_path(_path_id);
        throw std::runtime_error{std::format("Failed to seek in file stream: {}", path.string())};

    } else if (_file_stream.eof()) {
        // If the end of the file has been reached, we return 0.
        return 0;

    } else if (_file_stream.fail()) {
        // This may still have happened because of end-of-file. But the error
        // is recoverable. Force seek to the end of the file.
        _file_stream.clear(); // Clear the fail state.
        _file_stream.seekg(0, std::ios::end);
        if (_file_stream.fail() or _file_stream.bad()) {
            // If we cannot seek to the end of the file, we close it and throw an error.
            lock = _close(std::move(lock));
            auto const &path = get_path(_path_id);
            throw std::runtime_error{std::format("Failed to seek to end of file stream: {}", path.string())};
        }
    }

    _file_stream.read(buffer.data(), buffer.size());
    if (_file_stream.bad()) {
        // If the file stream is in a bad state, we close it and throw an error.
        lock = _close(std::move(lock));
        auto const &path = get_path(_path_id);
        throw std::runtime_error{std::format("Failed to read from file stream: {}", path.string())};

    } else if (_file_stream.eof()) {
        return 0;

    } else if (_file_stream.fail()) {
        lock = _close(std::move(lock));
        auto const &path = get_path(_path_id);
        throw std::runtime_error{std::format("Failed to read from file stream: {}", path.string())};
    }

    return _file_stream.gcount();
}

std::unique_lock<std::mutex> file_ifstream::_open(std::unique_lock<std::mutex> lock)
{
    assert(lock.owns_lock());

    if (_file_stream.is_open()) {
        return lock;
    }

    auto const &path = get_path(_path_id);
    _file_stream.open(path, std::ios::binary);
    if (_file_stream.fail() or _file_stream.bad()) {
        // If the file stream could not be opened, we throw an exception.
        // This is important to ensure that the file is not left in an
        // inconsistent state.
        throw std::runtime_error{std::format("Failed to open file stream: {}", path.string())};
    }
    // End-of-file is not an error, so we do not check for it here.
    return lock;
}

void file_ifstream::open()
{
    _open(std::unique_lock(_mutex));
}

std::unique_lock<std::mutex> file_ifstream::_close(std::unique_lock<std::mutex> lock) noexcept
{
    assert(lock.owns_lock());

    if (not _file_stream.is_open()) {
        return lock; // Nothing to close
    }

    _file_stream.close();
    _file_stream.clear();
    // Don't check for errors here, we can't really do anything about them.
    return lock;
}

void file_ifstream::close() noexcept
{
    _close(std::unique_lock(_mutex));
}

} // namespace hk