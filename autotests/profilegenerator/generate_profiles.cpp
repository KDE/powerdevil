/*
 *  SPDX-FileCopyrightText: 2023 Jakob Petsovits <jpetso@petsovits.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <powerdevilprofilegenerator.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char **argv)
{
    // Set XDG_CONFIG_HOME to ~/.qttest/config (and other XDG homes), which we're using to
    // temporarily store and access our config file before moving it to its destination path.
    QStandardPaths::setTestModeEnabled(true);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("generate_profiles");

    QCommandLineParser parser;
    parser.setApplicationDescription("Test helper: Generate a default powermanagementprofilesrc, but write to a different file.");
    parser.addPositionalArgument(
        QStringLiteral("dest_profilesrc"),
        QStringLiteral("Path of the newly converted config file for profile-specific settings. The PowerDevil daemon would ensure that there is an existing "
                       "powermanagementprofilesrc in your XDG_CONFIG_HOME, but this tool can produce a brand-new one under any name."));
    QCommandLineOption optionMobile("mobile",
                                    "Generate profiles for a mobile device (i.e. phones, tablets running Plasma Mobile) instead of regular desktop/laptop.");
    QCommandLineOption optionVM("vm", "Generate profiles for a virtual machine environment instead of bare metal.");
    QCommandLineOption optionCannotSuspendToRam("cannot-suspend-to-ram", "Assume that the device does not support suspending to RAM a.k.a. Sleep.");
    QCommandLineOption optionCannotSuspendToDisk("cannot-suspend-to-disk", "Assume that the device does not support suspending to disk a.k.a. Hibernate.");
    parser.addOptions({optionMobile, optionCannotSuspendToRam, optionCannotSuspendToDisk});
    parser.addHelpOption();
    parser.process(app);

    if (parser.positionalArguments().count() != 1) {
        parser.showHelp(1);
    }

    bool isMobile = parser.isSet(optionMobile);
    bool isVM = parser.isSet(optionVM);
    bool canSuspendToRam = !parser.isSet(optionCannotSuspendToRam);
    bool canSuspendToDisk = !parser.isSet(optionCannotSuspendToDisk);

    QString test_config_dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QString temp_profilesrc_path = test_config_dir + "/powermanagementprofilesrc";
    QString temp_globalrc_path = test_config_dir + "/powerdevilrc";

    QTimer::singleShot(0, [&] {
        // Successful or not, make sure we never end up with configs from a previous run.
        QFile::remove(temp_profilesrc_path);
        QFile::remove(temp_globalrc_path);
        QFile::remove(parser.positionalArguments()[0]);

        PowerDevil::ProfileGenerator::generateProfiles(isMobile, isVM, canSuspendToRam, canSuspendToDisk);

        if (!QFile::rename(temp_profilesrc_path, parser.positionalArguments()[0])) {
            qDebug() << "Unable to move config file to destination.";
            app.exit(1);
        }

        app.exit(0);
    });
    return app.exec();
}
