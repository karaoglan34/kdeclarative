/*
 *   Copyright 2015 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <QApplication>

#include <klocalizedstring.h>
#include <qcommandlineparser.h>
#include <qcommandlineoption.h>
#include <QQuickItem>

#include <kpackage/package.h>
#include <kpackage/packageloader.h>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQmlProperty>
#include <QQuickWindow>
#include <kdeclarative/qmlobject.h>
#include <KAboutData>

int main(int argc, char **argv)
{
    QCommandLineParser parser;
    QApplication app(argc, argv);

    const QString description = i18n("KPackage QML application shell");
    const char version[] = "0.1";

    app.setApplicationVersion(version);
    parser.addVersionOption();
    parser.addHelpOption();
    parser.setApplicationDescription(description);

    QCommandLineOption appPluginOption(QCommandLineOption(QStringList() << "a" << "app", i18n("The unique name of the application (mandatory)"), "app"));

    parser.addOption(appPluginOption);

    parser.process(app);

    QString packagePath;
    if (parser.isSet(appPluginOption)) {
        packagePath = parser.value(appPluginOption);
    } else {
        parser.showHelp(1);
    }

    //usually we have an ApplicationWindow here, so we do not need to create a window by ourselves
    KDeclarative::QmlObject obj;
    obj.setTranslationDomain(packagePath);
    obj.setInitializationDelayed(true);
    obj.loadPackage(packagePath);
    obj.engine()->rootContext()->setContextProperty("commandlineArguments", parser.positionalArguments());
    obj.completeInitialization();

    if (!obj.package().metadata().isValid()) {
        return -1;
    }

    KPluginMetaData data = obj.package().metadata();
    // About data
    KAboutData aboutData(data.pluginId(), data.name(), data.version(), data.description(), KAboutLicense::byKeyword(data.license()).key());

    for (auto author : data.authors()) {
        aboutData.addAuthor(author.name(), author.task(), author.emailAddress(), author.webAddress(), author.ocsUsername());
    }

    KAboutData::setApplicationData(aboutData);

    //The root is not a window?
    //have to use a normal QQuickWindow since the root item is already created
    QQuickItem *item = qobject_cast<QQuickItem *>(obj.rootObject());
    QWindow *window = qobject_cast<QWindow *>(obj.rootObject());
    if (item) {
        QQuickWindow view;
        item->setParentItem(view.contentItem());
        if (item->implicitWidth() > 0 && item->implicitHeight() > 0) {
            view.resize(item->implicitWidth(), item->implicitHeight());
        } else {
            view.resize(item->width(), item->height());
        }

        //set anchors
        QQmlExpression expr(obj.engine()->rootContext(), obj.rootObject(), "parent");
        QQmlProperty prop(obj.rootObject(), "anchors.fill");
        prop.write(expr.evaluate());
        view.setTitle(obj.package().metadata().name());
        view.setIcon(QIcon::fromTheme(obj.package().metadata().iconName()));

        view.show();
        return app.exec();
    } else if (window) {
        window->setTitle(obj.package().metadata().name());
        window->setIcon(QIcon::fromTheme(obj.package().metadata().iconName()));
    } else {
        qWarning() << "The root Qml Item should be either a kind of window or a QQuickItem";
        return 1;
    }

    return app.exec();
}
