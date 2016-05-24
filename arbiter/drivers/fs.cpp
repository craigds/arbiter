#ifndef ARBITER_IS_AMALGAMATION
#include <arbiter/arbiter.hpp>
#include <arbiter/drivers/fs.hpp>
#include <arbiter/util/util.hpp>
#endif

#ifndef ARBITER_WINDOWS
#include <glob.h>
#include <sys/stat.h>
#else

#include <locale>
#include <codecvt>
#include <windows.h>
#endif

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#ifdef ARBITER_CUSTOM_NAMESPACE
namespace ARBITER_CUSTOM_NAMESPACE
{
#endif

namespace arbiter
{

namespace
{
    // Binary output, overwriting any existing file with a conflicting name.
    const std::ios_base::openmode binaryTruncMode(
            std::ofstream::binary |
            std::ofstream::out |
            std::ofstream::trunc);

    const std::string home(([]()
    {
        std::string s;

#ifndef ARBITER_WINDOWS
        if (auto home = util::env("HOME")) s = *home;
#else
        if (auto userProfile = util::env("USERPROFILE"))
        {
            s = *userProfile;
        }
        else
        {
            auto homeDrive(util::env("HOMEDRIVE"));
            auto homePath(util::env("HOMEPATH"));

            if (homeDrive && homePath) s = homeDrive + homePath;
        }
#endif
        if (s.empty()) std::cout << "No home directory found" << std::endl;

        return s;
    })());
}

namespace drivers
{

std::unique_ptr<Fs> Fs::create(http::Pool&, const Json::Value&)
{
    return std::unique_ptr<Fs>(new Fs());
}

std::unique_ptr<std::size_t> Fs::tryGetSize(std::string path) const
{
    std::unique_ptr<std::size_t> size;

    path = fs::expandTilde(path);

    std::ifstream stream(path, std::ios::in | std::ios::binary);

    if (stream.good())
    {
        stream.seekg(0, std::ios::end);
        size.reset(new std::size_t(stream.tellg()));
    }

    return size;
}

bool Fs::get(std::string path, std::vector<char>& data) const
{
    bool good(false);

    path = fs::expandTilde(path);
    std::ifstream stream(path, std::ios::in | std::ios::binary);

    if (stream.good())
    {
        stream.seekg(0, std::ios::end);
        data.resize(static_cast<std::size_t>(stream.tellg()));
        stream.seekg(0, std::ios::beg);
        stream.read(data.data(), data.size());
        stream.close();
        good = true;
    }

    return good;
}

void Fs::put(std::string path, const std::vector<char>& data) const
{
    path = fs::expandTilde(path);
    std::ofstream stream(path, binaryTruncMode);

    if (!stream.good())
    {
        throw ArbiterError("Could not open " + path + " for writing");
    }

    stream.write(data.data(), data.size());

    if (!stream.good())
    {
        throw ArbiterError("Error occurred while writing " + path);
    }
}

std::vector<std::string> Fs::glob(std::string path, bool) const
{
    std::vector<std::string> results;

#ifndef ARBITER_WINDOWS
    path = fs::expandTilde(path);

    glob_t buffer;
    struct stat info;

    ::glob(path.c_str(), GLOB_NOSORT | GLOB_TILDE, 0, &buffer);

    for (std::size_t i(0); i < buffer.gl_pathc; ++i)
    {
        const std::string val(buffer.gl_pathv[i]);

        if (stat(val.c_str(), &info) == 0)
        {
            if (S_ISREG(info.st_mode))
            {
                results.push_back(val);
            }
        }
        else
        {
            throw ArbiterError("Error globbing - POSIX stat failed");
        }
    }

    globfree(&buffer);
#else
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    const std::wstring wide(converter.from_bytes(path));

    LPWIN32_FIND_DATAW data{};
    HANDLE hFind(FindFirstFileW(wide.c_str(), data));

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if ((data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                results.push_back(converter.to_bytes(data->cFileName));
            }
        }
        while (FindNextFileW(hFind, data));
    }
#endif

    return results;
}

} // namespace drivers

namespace fs
{

bool mkdirp(std::string dir)
{
    dir = expandTilde(dir);

#ifndef ARBITER_WINDOWS
    const bool err(::mkdir(dir.c_str(), S_IRWXU | S_IRGRP | S_IROTH));
    return (!err || errno == EEXIST);
#else
    throw ArbiterError("Windows mkdirp not done yet.");
#endif
}

bool remove(std::string filename)
{
    filename = expandTilde(filename);

#ifndef ARBITER_WINDOWS
    return ::remove(filename.c_str()) == 0;
#else
    throw ArbiterError("Windows remove not done yet.");
#endif
}

std::string expandTilde(std::string in)
{
    std::string out(in);

    if (!in.empty() && in.front() == '~')
    {
        if (home.empty()) throw ArbiterError("No home directory found");

        out = home + in.substr(1);
    }

    return out;
}

std::string getTempPath()
{
#ifndef ARBITER_WINDOWS
    if (const auto t = util::env("TMPDIR"))     return *t;
    if (const auto t = util::env("TMP"))        return *t;
    if (const auto t = util::env("TEMP"))       return *t;
    if (const auto t = util::env("TEMPDIR"))    return *t;
    return "/tmp";
#else
    std::vector<char> path(MAX_PATH, '\0');
    if (GetTempPath(MAX_PATH, path.data())) return path.data();
    else throw ArbiterError("Could not find a temp path.");
#endif
}

LocalHandle::LocalHandle(const std::string localPath, const bool isRemote)
    : m_localPath(expandTilde(localPath))
    , m_erase(isRemote)
{ }

LocalHandle::~LocalHandle()
{
    if (m_erase) fs::remove(fs::expandTilde(m_localPath));
}

} // namespace fs
} // namespace arbiter

#ifdef ARBITER_CUSTOM_NAMESPACE
}
#endif

