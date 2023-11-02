/*
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <powerdevilmigrateconfig.h>

#include <KSharedConfig>
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char **argv)
{
    // Set XDG_CONFIG_HOME to ~/.qttest/config (and other XDG homes), which we're using to
    // temporarily store and access our config file before moving it to its destination path.
    QStandardPaths::setTestModeEnabled(true);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("migrateconfig");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Test helper: Migrate an existing Plasma 5 PowerDevil configuration its Plasma 6 equivalent, but write the output to a user-specified path.");

    parser.addOption(
        {"src-powerdevilrc",
         "Path of an original global configuration file from Plasma 5 times. Doesn't have to be called powerdevilrc, but will be interpreted as such.",
         "path"});
    parser.addOption({"src-profilesrc",
                      "Path of an original profile configuration file from Plasma 5 times. Doesn't have to be called powermanagementprofilesrc, but will be "
                      "interpreted as such.",
                      "path"});
    parser.addOption({"dest-powerdevilrc",
                      "Path of the newly migrated config file for global settings. The PowerDevil daemon would overwrite the existing powerdevilrc, but this "
                      "tool can save it under any name.",
                      "path"});
    parser.addOption({"dest-profilesrc",
                      "Path of the newly migrated config file for profile-specific settings. The PowerDevil daemon would overwrite the existing "
                      "powermanagementprofilesrc, but this tool can save it under any name.",
                      "path"});

    parser.addOption({"assert-no-powerdevilrc-after-migration", "Abort with a non-zero error code if powerdevilrc exists after migrating."});

    parser.addOption({"mobile", "Assume running on a mobile device (i.e. phones, tablets running Plasma Mobile) instead of regular desktop/laptop."});
    parser.addOption({"vm", "Assume running in a virtual machine environment instead of bare metal."});
    parser.addOption({"cannot-suspend", "Assume that the device does not support suspending to RAM a.k.a. Sleep."});

    parser.addHelpOption();
    parser.process(app);

    bool assertNoPowerdevilrcAfterMigration = parser.isSet("assert-no-powerdevilrc-after-migration");
    bool isMobile = parser.isSet("mobile");
    bool isVM = parser.isSet("vm");
    bool canSuspend = !parser.isSet("cannot-suspend");

    QString src_powerdevilrc_path = parser.value("src-powerdevilrc");
    QString src_profilesrc_path = parser.value("src-profilesrc");
    QString dest_powerdevilrc_path = parser.value("dest-powerdevilrc");
    QString dest_profilesrc_path = parser.value("dest-profilesrc");

    QString test_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QString temp_powerdevilrc_path = test_config_dir + "/powerdevilrc";
    QString temp_profilesrc_path = test_config_dir + "/powermanagementprofilesrc";

    QTimer::singleShot(0, [&] {
        // Successful or not, make sure we never end up with configs from a previous run.
        if (!dest_powerdevilrc_path.isEmpty()) {
            QFile::remove(dest_powerdevilrc_path);
        }
        if (!dest_profilesrc_path.isEmpty()) {
            QFile::remove(dest_profilesrc_path);
        }
        QFile::remove(temp_powerdevilrc_path);
        QFile::remove(temp_profilesrc_path);

        if (!src_powerdevilrc_path.isEmpty()) {
            if (!QFile::copy(src_powerdevilrc_path, temp_powerdevilrc_path)) {
                qDebug() << "Unable to copy config file to its temporary location.";
                app.exit(1);
            }
        }
        if (!src_profilesrc_path.isEmpty()) {
            if (!QFile::copy(src_profilesrc_path, temp_profilesrc_path)) {
                qDebug() << "Unable to copy config file to its temporary location.";
                app.exit(1);
            }
        }

        PowerDevil::migrateConfig(isMobile, isVM, canSuspend);

        if (assertNoPowerdevilrcAfterMigration && QFile::exists(temp_powerdevilrc_path)) {
            qDebug() << "Unexpected powerdevilrc exists after migration.";
            app.exit(1);
            return;
        }
        if (!dest_powerdevilrc_path.isEmpty()) {
            if (!QFile::rename(temp_powerdevilrc_path, dest_powerdevilrc_path)) {
                qDebug() << "Unable to move global config file to destination.";
                app.exit(1);
                return;
            }
        }
        if (!dest_profilesrc_path.isEmpty()) {
            if (!QFile::rename(temp_profilesrc_path, dest_profilesrc_path)) {
                qDebug() << "Unable to move profile config file to destination.";
                app.exit(1);
                return;
            }
        }

        app.exit(0);
    });
    return app.exec();
}
