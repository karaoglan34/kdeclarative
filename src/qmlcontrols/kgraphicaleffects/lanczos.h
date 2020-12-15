/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include <QQuickFramebufferObject>

/**
 * Smoothly resize a source.
 *
 * Downscaling or upscaling  will introduce artefacts. This item provides a resize that is both smooth but not blurry.
 * Internally it is powered by a Lanczos filter.
 *
 * This is more costly than a resize, but the results are generally better.
 *
 * Usage is similar to ShaderEffect.
 * OpenGL is required to be operational. Otherwise no contents will be shown.
 *
 * Example usage:
 * @code
 *
  SomeItem {
    id: myContents
    layer.enabled: true
  }

  SmoothResize {
   source: myContent
   anchors.fill: myContents
  }


   @endcode
 *
 */
class Lanczos: public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem *source READ source WRITE setSource NOTIFY sourceChanged)

public:
    Lanczos();
    Renderer *createRenderer() const;

    QQuickItem *source();
    /**
     * The source to run the effect on.
     * This source must be a texture provider, such as Image or ShaderEffectSource
     *
     * Aspect ratio is not explicitly maintained if the source and this item differ.
     *
     * The current implementation does not support atlas textures.
     */
    void setSource(QQuickItem *source);
Q_SIGNALS:
    void sourceChanged();
private:
    QPointer<QQuickItem> m_source;
};

