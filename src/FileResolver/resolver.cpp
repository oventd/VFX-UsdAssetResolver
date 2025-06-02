#define CONVERT_STRING(string) #string
#define DEFINE_STRING(string) CONVERT_STRING(string)

#include "resolver.h"
#include "resolverContext.h"

#include "pxr/base/arch/systemInfo.h"
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/pyInvoke.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/usd/ar/filesystemAsset.h"
#include "pxr/usd/ar/filesystemWritableAsset.h"
#include "pxr/usd/ar/notice.h"
#include <pxr/usd/ar/defaultResolver.h>

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <regex>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEBUG_CODES(
    AR_RESOLVER_INIT
)

AR_DEFINE_RESOLVER(FileResolver, ArResolver);

static bool
_IsRelativePath(const std::string& path)
{
    return (!path.empty() && TfIsRelativePath(path));
}

static bool
_IsFileRelativePath(const std::string& path) {
    return path.find("./") == 0 || path.find("../") == 0;
}

static bool
_IsSearchPath(const std::string& path)
{
    return _IsRelativePath(path) && !_IsFileRelativePath(path);
}

static std::string
_AnchorRelativePath(
    const std::string& anchorPath, 
    const std::string& path)
{
    if (TfIsRelativePath(anchorPath) ||
        !_IsRelativePath(path)) {
        return path;
    }
    // Ensure we are using forward slashes and not back slashes.
    std::string forwardPath = anchorPath;
    std::replace(forwardPath.begin(), forwardPath.end(), '\\', '/');
    // If anchorPath does not end with a '/', we assume it is specifying
    // a file, strip off the last component, and anchor the path to that
    // directory.
    const std::string anchoredPath = TfStringCatPaths(TfStringGetBeforeSuffix(forwardPath, '/'), path);
    return TfNormPath(anchoredPath);
}

static ArResolvedPath
_ResolveAnchored(
    const std::string& anchorPath,
    const std::string& path)
{
    std::string resolvedPath = path;
    if (!anchorPath.empty()) {
        resolvedPath = TfStringCatPaths(anchorPath, path);
    }
    return TfPathExists(resolvedPath) ? ArResolvedPath(TfAbsPath(resolvedPath)) : ArResolvedPath();
}

FileResolver::FileResolver() {
    this->SetExposeAbsolutePathIdentifierState(TfGetenvBool(DEFINE_STRING(AR_FILERESOLVER_ENV_EXPOSE_ABSOLUTE_PATH_IDENTIFIERS), false));

};
    
FileResolver::~FileResolver() = default;

std::string
FileResolver::_CreateIdentifier(
    const std::string& assetPath,
    const ArResolvedPath& anchorAssetPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg("Resolver::_CreateIdentifier('%s', '%s')\n",
                                        assetPath.c_str(), anchorAssetPath.GetPathString().c_str());

    if (assetPath.empty()) {
        return assetPath;
    }

    if (!anchorAssetPath) {
        return TfNormPath(assetPath);
    }

    const std::string anchoredAssetPath = _AnchorRelativePath(anchorAssetPath, assetPath);

    if (_IsSearchPath(assetPath) && Resolve(anchoredAssetPath).empty()) {
        return TfNormPath(assetPath);
    }

    return TfNormPath(anchoredAssetPath);
}

std::string
FileResolver::_CreateIdentifierForNewAsset(
    const std::string& assetPath,
    const ArResolvedPath& anchorAssetPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::_CreateIdentifierForNewAsset"
        "('%s', '%s')\n",
        assetPath.c_str(), anchorAssetPath.GetPathString().c_str());
    if (assetPath.empty()) {
        return assetPath;
    }

    if (_IsRelativePath(assetPath)) {
        return TfNormPath(anchorAssetPath ? 
            _AnchorRelativePath(anchorAssetPath, assetPath) :
            TfAbsPath(assetPath));
    }

    return TfNormPath(assetPath);
}

ArResolvedPath
FileResolver::_Resolve(const std::string& assetPath) const
{
    if (assetPath.empty()) {
        return ArResolvedPath();
    }
    TF_DEBUG(AR_RESOLVER_INIT).Msg(">>> Resolve called for: %s\n", assetPath.c_str());
    if (TfStringStartsWith(assetPath, "rsrc:/")) {
        std::cout << ">>> FileResolver Resolve calleddd" << std::endl;
        printf(">>> Resolve called fordd: %s\n", assetPath.c_str());
        TF_DEBUG(AR_RESOLVER_INIT).Msg(">>> Resolve called fordd: %s\n", assetPath.c_str());

        const char* show = std::getenv("SHOW_NAME");
        const char* shot = std::getenv("SHOT_NAME");

        if (!show || !shot) {
            TF_WARN("SHOW_NAME or SHOT_NAME is not set");
            return ArResolvedPath();
        }

        std::string relativePath = assetPath.substr(strlen("rsrc:/"));
        if (!relativePath.empty() && relativePath[0] == '/') {
            relativePath = relativePath.substr(1);
        }

        std::string resolvedPath = TfStringPrintf(
            "D:/inferno/%s/%s/%s",
            show, shot, relativePath.c_str()
        );

        TF_DEBUG(AR_RESOLVER_INIT).Msg(">>> FileResolver: %s > %s\n",
                                       assetPath.c_str(), resolvedPath.c_str());

        return ArResolvedPath(resolvedPath);
    }

    pxr::ArDefaultResolver defaultResolver;
    return defaultResolver.Resolve(assetPath);
}

ArResolvedPath
FileResolver::_ResolveForNewAsset(
    const std::string& assetPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg("Resolver::_ResolveForNewAsset('%s')\n", assetPath.c_str());
    return ArResolvedPath(assetPath.empty() ? assetPath : TfAbsPath(assetPath));
}

ArResolverContext
FileResolver::_CreateDefaultContext() const
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContext()\n");
    return _fallbackContext;
}

ArResolverContext
FileResolver::_CreateDefaultContextForAsset(
    const std::string& assetPath) const
{
    // As there can be only one context class instance per context class
    // we automatically skip creation of contexts if it exists (The performance heavy
    // part is the pinning data creation). Here we can return any existing instance of
    // a FileResolverContext, thats why we just use the fallback context.
    // See for more info: https://openusd.org/release/api/class_ar_resolver_context.html
    // > Note that an ArResolverContext may not hold multiple context objects with the same type.
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s')\n", assetPath.c_str());
    // Fallback to existing context
    if (assetPath.empty()){
        return ArResolverContext(_fallbackContext);
    }
    ArResolvedPath resolvedPath = this->_Resolve(assetPath);
    if (!TfPathExists(resolvedPath)){
        return ArResolverContext(_fallbackContext);
    }
    if (this->_GetCurrentContextPtr() != nullptr)
    {
        if (TfDebug::IsEnabled(FILERESOLVER_RESOLVER_CONTEXT))
        {
            if (this->_GetCurrentContextPtr() == &_fallbackContext)
            {
                TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Reusing already bound default context on same stage\n", assetPath.c_str());
            }
            else
            {
                TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Reusing already bound context CachedResolverContext('%s') on same stage\n", assetPath.c_str(), this->_GetCurrentContextPtr()->GetMappingFilePath().c_str());
            }
        }
        return ArResolverContext(*this->_GetCurrentContextPtr());
    }
    auto map_iter = _sharedContexts.find(resolvedPath);
    if(map_iter != _sharedContexts.end()){
        if (map_iter->second.timestamp.GetTime() == this->_GetModificationTimestamp(assetPath, resolvedPath).GetTime())
        {
            TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Reusing context on different stage\n", assetPath.c_str());
            return ArResolverContext(map_iter->second.ctx);
        }else{
            TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Reusing context on different stage, reloading due to changed timestamp\n", assetPath.c_str());
            map_iter->second.ctx.RefreshFromMappingFilePath();
            return ArResolverContext(map_iter->second.ctx);
        }
    }
    // Create new context
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_CreateDefaultContextForAsset('%s') - Constructing new context\n", assetPath.c_str());
    std::string resolvedPathStr = resolvedPath.GetPathString();
    std::string assetDir = TfGetPathName(TfAbsPath(resolvedPathStr));
    struct FileResolverContextRecord record
    {
        this->_GetModificationTimestamp(assetPath, resolvedPath), FileResolverContext(resolvedPath, std::vector<std::string>(1, assetDir))
    };
    _sharedContexts.insert(std::pair<std::string, FileResolverContextRecord>(resolvedPath, record));
    return ArResolverContext(record.ctx);
}

bool
FileResolver::_IsContextDependentPath(
    const std::string& assetPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_IsContextDependentPath()\n");
    if (this->exposeAbsolutePathIdentifierState){
        return true;
    }
    return _IsSearchPath(assetPath);
}

void
FileResolver::_RefreshContext(
    const ArResolverContext& context)
{
    TF_DEBUG(FILERESOLVER_RESOLVER_CONTEXT).Msg("Resolver::_RefreshContext()\n");
    const FileResolverContext* ctx = this->_GetCurrentContextPtr();
    if (!ctx) {
        return;
    }
    ArNotice::ResolverChanged(*ctx).Send();
}

ArTimestamp
FileResolver::_GetModificationTimestamp(
    const std::string& assetPath,
    const ArResolvedPath& resolvedPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::GetModificationTimestamp('%s', '%s')\n",
        assetPath.c_str(), resolvedPath.GetPathString().c_str());
    return ArFilesystemAsset::GetModificationTimestamp(resolvedPath);
}

std::shared_ptr<ArAsset>
FileResolver::_OpenAsset(
    const ArResolvedPath& resolvedPath) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::OpenAsset('%s')\n",
        resolvedPath.GetPathString().c_str());
    return ArFilesystemAsset::Open(resolvedPath);
}

std::shared_ptr<ArWritableAsset>
FileResolver::_OpenAssetForWrite(
    const ArResolvedPath& resolvedPath,
    WriteMode writeMode) const
{
    TF_DEBUG(FILERESOLVER_RESOLVER).Msg(
        "Resolver::_OpenAssetForWrite('%s', %d)\n",
        resolvedPath.GetPathString().c_str(),
        static_cast<int>(writeMode));
    return ArFilesystemWritableAsset::Create(resolvedPath, writeMode);
}

const FileResolverContext* 
FileResolver::_GetCurrentContextPtr() const
{
    return _GetCurrentContextObject<FileResolverContext>();
}

PXR_NAMESPACE_CLOSE_SCOPE