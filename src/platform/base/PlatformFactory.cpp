#include "PlatformFactory.h"
#include "PortableNetworkPlatform.h"
#include <QCryptographicHash>
#include <QSysInfo>

namespace netraxis {
BackendKind currentBackendKind(){
#if defined(Q_OS_WIN)
    return BackendKind::Windows;
#elif defined(Q_OS_MACOS)
    return BackendKind::MacOS;
#elif defined(Q_OS_LINUX)
    return BackendKind::Linux;
#else
#error Netraxis requires Windows, Linux or macOS
#endif
}
Result<std::shared_ptr<INetworkPlatform>> createNetworkPlatform(){return createNetworkPlatform(currentBackendKind());}
Result<std::shared_ptr<INetworkPlatform>> createNetworkPlatform(BackendKind requested){
    if(requested!=currentBackendKind())return Result<std::shared_ptr<INetworkPlatform>>::failure({ErrorCode::Unsupported,"A backend for another operating system cannot run on this host.",false,"platform"});
    auto identity=QSysInfo::machineUniqueId();
    if(identity.isEmpty())identity=QSysInfo::machineHostName().toUtf8()+QSysInfo::productType().toUtf8();
    if(identity.isEmpty())return Result<std::shared_ptr<INetworkPlatform>>::failure({ErrorCode::Internal,"Cannot identify the local backend.",false,"platform"});
    const auto host=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    QString name;
    switch(requested){case BackendKind::Windows:name="Windows";break;case BackendKind::Linux:name="Linux";break;case BackendKind::MacOS:name="macOS";break;}
    PlatformDescriptor descriptor{requested,name,"Qt Network native interface provider",host};
    return Result<std::shared_ptr<INetworkPlatform>>::success(std::make_shared<PortableNetworkPlatform>(descriptor));
}
}
