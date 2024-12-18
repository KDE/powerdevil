add_definitions(-DTRANSLATION_DOMAIN=\"kcm_powerdevilprofilesconfig\")

kcmutils_add_qml_kcm(kcm_powerdevilprofilesconfig)

target_sources(kcm_powerdevilprofilesconfig PRIVATE
    PowerKCM.cpp PowerKCM.h
    ExternalServiceSettings.cpp ExternalServiceSettings.h
    PowerButtonActionModel.cpp PowerButtonActionModel.h
    PowerProfileModel.cpp PowerProfileModel.h
    SleepModeModel.cpp SleepModeModel.h
)

target_link_libraries(kcm_powerdevilprofilesconfig
    Qt::DBus
    KF6::AuthCore
    KF6::KCMUtils
    KF6::Kirigami2
    KF6::I18n
    KF6::ItemModels
    KF6::Solid
    powerdevilcore
)
