#pragma once
#include "trace.h"
#include <windows.h>
#include <string>

class file_stream
{
public:
    file_stream() :
        _handle(INVALID_HANDLE_VALUE)
    {
    }

    ~file_stream()
    {
        close();
    }

    void open(const std::string& path,
        DWORD create_mode, DWORD access_mode, DWORD share_mode = 0)
    {
        close();

        _handle = CreateFileA(path.c_str(), access_mode, share_mode,
            NULL, create_mode, FILE_ATTRIBUTE_NORMAL, NULL);

        if (_handle == INVALID_HANDLE_VALUE)
            throw std::exception("file_stream, open file");
    }

    void close()
    {
        if (_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(_handle);
            _handle = INVALID_HANDLE_VALUE;
        }
    }

    DWORD read(void* data, DWORD count)
    {
        check_handle();

        DWORD read_count = 0;
        if (ReadFile(_handle, data, count, &read_count, NULL))
            return read_count;

        throw std::exception("file_stream, read file");
    }

    DWORD write(const void* data, DWORD count)
    {
        check_handle();

        DWORD write_count = 0;
        if (WriteFile(_handle, data, count, &write_count, NULL))
            return write_count;

        throw std::exception("file_stream, write file");
    }

    DWORD seek(LONG length, DWORD method)
    {
        check_handle();

        DWORD ret = SetFilePointer(_handle, length, NULL, method);
        if (ret != INVALID_SET_FILE_POINTER)
            return ret;

        throw std::exception("file_stream, seek");
    }

    DWORD size()
    {
        check_handle();

        DWORD ret = GetFileSize(_handle, NULL);
        if (ret != INVALID_FILE_SIZE)
            return ret;

        throw std::exception("file_stream, size");
    }

    DWORD position()
    {
        check_handle();

        DWORD ret = SetFilePointer(_handle, 0, NULL, FILE_CURRENT);
        if (ret != INVALID_SET_FILE_POINTER)
            return ret;

        throw std::exception("file_stream, position");
    }

    void flush()
    {
        check_handle();

        if (FlushFileBuffers(_handle))
            return;

        throw std::exception("file_stream, flush");
    }

    void truncate()
    {
        check_handle();

        if (SetEndOfFile(_handle))
            return;

        throw std::exception("file_stream, truncate");
    }
private:
    void check_handle()
    {
        if (_handle == INVALID_HANDLE_VALUE)
            throw std::exception("file_stream, invalid handle");
    }

    HANDLE _handle;
private:
    file_stream(const file_stream&);
    file_stream& operator=(const file_stream&);
};
