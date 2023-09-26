add_definitions(-DTRANSLATION_DOMAIN=\"kcm_powerdevilprofilesconfig\")

kcmutils_add_qml_kcm(kcm_powerdevilprofilesconfig)

target_sources(kcm_powerdevilprofilesconfig PRIVATE
    ProfilesConfigKCM.cpp
    ${kcm_powerdevil_profiles_SRCS}
)

target_link_libraries(kcm_powerdevilprofilesconfig
    KF6::KCMUtils
    KF6::I18n
    KF6::ItemModels
    KF6::Kirigami2
    powerdevilconfigcommonprivate
)
