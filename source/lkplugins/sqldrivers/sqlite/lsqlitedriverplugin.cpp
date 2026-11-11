#include "lksql/lsqldriverplugin.h"
#include "lsqlitedriver.h"
#include <limits.h> // defines PATH_MAX on unix
#include <sqlite3.h>
#include <stdio.h> // defines FILENAME_MAX everywhere
#include "lkcore/io/liofileman.h"

#ifndef PATH_MAX
#  define PATH_MAX FILENAME_MAX
#endif
#if SQLITE_VERSION_NUMBER < 3040000
typedef const char *sqlite3_filename;
#endif


LK_NAMESPACE_BEGIN


namespace lkSqlite{
struct Vfs : sqlite3_vfs {
    sqlite3_vfs *pVfs;
    sqlite3_io_methods ioMethods;
};

struct xFile : sqlite3_file {
    class SqliteFileMan : public LIOFileMan {
    public:
        SqliteFileMan(const std::string &name, bool removeOnClose)
            : LIOFileMan(name)
            , removeOnClose(removeOnClose)
        {}

        SqliteFileMan(bool removeOnClose) : LIOFileMan() , removeOnClose(removeOnClose)
        {}

        ~SqliteFileMan()
        {
            close();
            if (removeOnClose){
                remove();
            }
        }
    private:
        bool removeOnClose;
    };
    std::shared_ptr<SqliteFileMan> pMan;
};


int xClose(sqlite3_file *sfile)
{
    auto file = static_cast<xFile *>(sfile);
    file->pMan = nullptr;
    return SQLITE_OK;
}

int xRead(sqlite3_file *sfile, void *ptr, int iAmt, sqlite3_int64 iOfst)
{
    auto file = static_cast<xFile *>(sfile);
    if (!file->pMan->seekg(iOfst)){
        return SQLITE_IOERR_READ;
    }
    auto sz = file->pMan->readRawData(static_cast<uint8_t *>(ptr), iAmt);
    if (sz < iAmt) {
        memset(static_cast<char *>(ptr) + sz, 0, size_t(iAmt - sz));
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

int xWrite(sqlite3_file *sfile, const void *data, int iAmt, sqlite3_int64 iOfst)
{
    auto file = static_cast<xFile *>(sfile);
    if (!file->pMan->seekp(iOfst)){
        return SQLITE_IOERR_SEEK;
    }
    return file->pMan->writeRawData(reinterpret_cast<const uint8_t*>(data), iAmt) ? SQLITE_OK : SQLITE_IOERR_WRITE;
}

int xTruncate(sqlite3_file *sfile, sqlite3_int64 size)
{
    auto file = static_cast<xFile *>(sfile);
    return file->pMan->resize(size) ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
}

int xSync(sqlite3_file *sfile, int /*flags*/)
{
    static_cast<xFile *>(sfile)->pMan->flush();
    return SQLITE_OK;
}

int xFileSize(sqlite3_file *sfile, sqlite3_int64 *pSize)
{
    auto file = static_cast<xFile *>(sfile);
    *pSize = file->pMan->size();
    return SQLITE_OK;
}

int xLock(sqlite3_file *, int) { return SQLITE_OK; }

int xUnlock(sqlite3_file *, int) { return SQLITE_OK; }

int xCheckReservedLock(sqlite3_file *, int *pResOut)
{
    *pResOut = 0;
    return SQLITE_OK;
}

int xFileControl(sqlite3_file *, int, void *) { return SQLITE_NOTFOUND; }

int xSectorSize(sqlite3_file *)
{
    return 4096;
}

int xDeviceCharacteristics(sqlite3_file *)
{
    return 0; // no SQLITE_IOCAP_XXX
}

int xOpen(sqlite3_vfs *svfs, sqlite3_filename zName, sqlite3_file *sfile,
          int flags, int *pOutFlags)
{
    auto vfs = static_cast<Vfs *>(svfs);
    xFile* file = static_cast<xFile *>(sfile);
    file->pMan = nullptr;
    file->pMethods = nullptr;
    int mode = std::ios_base::binary;
    if (!zName || (flags & SQLITE_OPEN_MEMORY)){
        return SQLITE_PERM;
    }

    if ((flags & SQLITE_OPEN_READONLY) &&
        !(flags & SQLITE_OPEN_READWRITE) &&
        !(flags & SQLITE_OPEN_CREATE) &&
        !(flags & SQLITE_OPEN_DELETEONCLOSE)) {
        mode |= std::ios_base::in;
    } else {
        /*
            ** ^The [SQLITE_OPEN_EXCLUSIVE] flag is always used in conjunction
            ** with the [SQLITE_OPEN_CREATE] flag, which are both directly
            ** analogous to the O_EXCL and O_CREAT flags of the POSIX open()
            ** API.  The SQLITE_OPEN_EXCLUSIVE flag, when paired with the
            ** SQLITE_OPEN_CREATE, is used to indicate that file should always
            ** be created, and that it is an error if it already exists.
            ** It is <i>not</i> used to indicate the file should be opened
            ** for exclusive access.
         */
        if ((flags & SQLITE_OPEN_CREATE) && (flags & SQLITE_OPEN_EXCLUSIVE)){
            mode |= std::ios_base::noreplace;
        }

        if (flags & SQLITE_OPEN_READWRITE){
            mode |= std::ios_base::in|std::ios_base::out;
        }
    }

    file->pMethods = &vfs->ioMethods;
    file->pMan = std::make_shared<xFile::SqliteFileMan>(bool(flags & SQLITE_OPEN_DELETEONCLOSE));
    if (!file->pMan->open(std::string(zName), mode)){
        return SQLITE_CANTOPEN;
    }
    if (pOutFlags){
        *pOutFlags = flags;
    }

    return SQLITE_OK;
}

int xDelete(sqlite3_vfs *, const char *zName, int)
{
    return std::filesystem::remove(zName) ? SQLITE_OK : SQLITE_ERROR;
}

int xAccess(sqlite3_vfs */*svfs*/, const char *zName, int flags, int *pResOut)
{
    *pResOut = 0;
    switch (flags) {
    case SQLITE_ACCESS_EXISTS:
    case SQLITE_ACCESS_READ:{
        *pResOut = std::filesystem::exists(zName);
        break;
    }
    default:{
        break;
    }
    }
    return SQLITE_OK;
}

int xFullPathname(sqlite3_vfs *, const char *zName, int nOut, char *zOut)
{
    if (!zName){
        return SQLITE_ERROR;
    }

    int i = 0;
    for (;zName[i] && i < nOut; ++i){
        zOut[i] = zName[i];
    }

    if (i >= nOut){
        return SQLITE_ERROR;
    }

    zOut[i] = '\0';
    return SQLITE_OK;
}

int xRandomness(sqlite3_vfs *svfs, int nByte, char *zOut)
{
    auto vfs = static_cast<Vfs *>(svfs)->pVfs;
    return vfs->xRandomness(vfs, nByte, zOut);
}

int xSleep(sqlite3_vfs *svfs, int microseconds)
{
    auto vfs = static_cast<Vfs *>(svfs)->pVfs;
    return vfs->xSleep(vfs, microseconds);
}

int xCurrentTime(sqlite3_vfs *svfs, double *zOut)
{
    auto vfs = static_cast<Vfs *>(svfs)->pVfs;
    return vfs->xCurrentTime(vfs, zOut);
}

int xGetLastError(sqlite3_vfs *, int, char *)
{
    return 0;
}

int xCurrentTimeInt64(sqlite3_vfs *svfs, sqlite3_int64 *zOut)
{
    auto vfs = static_cast<Vfs *>(svfs)->pVfs;
    return vfs->xCurrentTimeInt64(vfs, zOut);
}
}


class LSqliteDriverPlugin : public LSqlDriverPlugin
{
public:
    LSqliteDriverPlugin();

    virtual std::shared_ptr<LSqlDriver> create()override;
};

LSqliteDriverPlugin::LSqliteDriverPlugin() : LSqlDriverPlugin()
{
    static lkSqlite::Vfs vfs;
    memset(&vfs, 0, sizeof(lkSqlite::Vfs));
    vfs.iVersion = 1;
    vfs.szOsFile = sizeof(lkSqlite::xFile);
    vfs.mxPathname = PATH_MAX;
    vfs.zName = "LkVFS";
    vfs.xOpen = &lkSqlite::xOpen;
    vfs.xDelete = &lkSqlite::xDelete;
    vfs.xAccess = &lkSqlite::xAccess;
    vfs.xFullPathname = &lkSqlite::xFullPathname;
    vfs.xRandomness = &lkSqlite::xRandomness;
    vfs.xSleep = &lkSqlite::xSleep;
    vfs.xCurrentTime = &lkSqlite::xCurrentTime;
    vfs.xGetLastError = &lkSqlite::xGetLastError;
    vfs.xCurrentTimeInt64 = &lkSqlite::xCurrentTimeInt64;
    vfs.pVfs = sqlite3_vfs_find(nullptr);
    vfs.ioMethods.iVersion = 1;
    vfs.ioMethods.xClose = &lkSqlite::xClose;
    vfs.ioMethods.xRead = &lkSqlite::xRead;
    vfs.ioMethods.xWrite = &lkSqlite::xWrite;
    vfs.ioMethods.xTruncate = &lkSqlite::xTruncate;
    vfs.ioMethods.xSync = &lkSqlite::xSync;
    vfs.ioMethods.xFileSize = &lkSqlite::xFileSize;
    vfs.ioMethods.xLock = &lkSqlite::xLock;
    vfs.ioMethods.xUnlock = &lkSqlite::xUnlock;
    vfs.ioMethods.xCheckReservedLock = &lkSqlite::xCheckReservedLock;
    vfs.ioMethods.xFileControl = &lkSqlite::xFileControl;
    vfs.ioMethods.xSectorSize = &lkSqlite::xSectorSize;
    vfs.ioMethods.xDeviceCharacteristics = &lkSqlite::xDeviceCharacteristics;

    sqlite3_vfs_register(&vfs, 0);
}

std::shared_ptr<LSqlDriver>  LSqliteDriverPlugin::create()
{
    std::shared_ptr<LSqlDriver> driver = std::make_shared<LSqliteDriver>();
    return driver;
}
LK_NAMESPACE_END

_LPLUGIN_ENTRY_DEFINE(_LK::LSqliteDriverPlugin)
