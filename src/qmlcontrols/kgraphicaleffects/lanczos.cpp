/*
    SPDX-FileCopyrightText: 2020 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "lanczos.h"

#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLShaderProgram>
#include <QOpenGLContext>
#include <QOpenGLBuffer>
#include <QSGTextureProvider>

#include <QVector2D>
#include <QVector4D>
#include <QtMath>

#include <QOpenGLFunctions>

#include <cmath>

class LanczosRenderer : public QObject, public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    LanczosRenderer();
    void render() override;
    void synchronize(QQuickFramebufferObject * ) override;

private:
    void createGeometry();
    QVector<QVector4D> createKernel(float delta, int *size);
    QVector<QVector2D> createOffsets(int count, float width, Qt::Orientation direction);
    QOpenGLShaderProgram m_program;

    QVector<QVector2D> m_vertices;
    QVector<QVector2D> m_texCoords;
    QPointer<QSGTextureProvider> m_source;
};

static float sinc(float x)
{
    return std::sin(x * M_PI) / (x * M_PI);
}

static float lanczos(float x, float a)
{
    if (qFuzzyIsNull(x))
        return 1.0;

    if (qAbs(x) >= a)
        return 0.0;

    return sinc(x) * sinc(x / a);
}

QVector<QVector4D> LanczosRenderer::createKernel(float delta, int *size)
{
    const float a = 2.0;
    QVector<QVector4D> kernel;
    kernel.fill(QVector4D(), 16);

    // The two outermost samples always fall at points where the lanczos
    // function returns 0, so we'll skip them.
    const int sampleCount = qBound(3, qCeil(delta * a) * 2 + 1 - 2, 29);
    const int center = sampleCount / 2;
    const int kernelSize = center + 1;
    const float factor = 1.0 / delta;

    QVector<float> values(kernelSize);
    float sum = 0;

    for (int i = 0; i < kernelSize; i++) {
        const float val = lanczos(i * factor, a);
        sum += i > 0 ? val * 2 : val;
        values[i] = val;
    }

    // Normalize the kernel
    for (int i = 0; i < kernelSize; i++) {
        const float val = values[i] / sum;
        kernel[i] = QVector4D(val, val, val, val);
    }

    *size = kernelSize;
    return kernel;
}

QVector<QVector2D> LanczosRenderer::createOffsets(int count, float width, Qt::Orientation direction)
{
    QVector<QVector2D> offsets;
    offsets.fill(QVector2D(), 16);
    for (int i = 0; i < count; i++) {
        offsets[i] = (direction == Qt::Horizontal) ?
            QVector2D(i / width, 0) : QVector2D(0, i / width);
    }
    return offsets;
}

void LanczosRenderer::synchronize(QQuickFramebufferObject* parent) {
    auto lancsoz = static_cast<Lanczos*>(parent);
    QQuickItem* texSource = lancsoz->source();
    if (texSource) {
        if (m_source) {
            disconnect(m_source.data(), nullptr, this, nullptr);
        }
        m_source = texSource->textureProvider();

        // we connect inside the LanczosRenderer not Lanczos  as textureProvider can only be used on the render thread
        connect(m_source.data(), &QSGTextureProvider::textureChanged, this, [this]() {
            update();
        }, Qt::DirectConnection);
        update();
    } else {
        m_source.clear();
    }
}

LanczosRenderer::LanczosRenderer()
{

    initializeOpenGLFunctions();
     m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,  ":/lanczos-fragment.vert");
    m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,  ":/lanczos-fragment.frag");

    createGeometry();

    m_program.link();

    m_program.bind();
    m_program.setUniformValue("uTex", 0);
}

void LanczosRenderer::render()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    if (!m_source || ! m_source->texture()) {
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    auto texture = m_source->texture();

    m_program.bind();

    QSize sourceSize = texture->textureSize();
    QSize targetSize = framebufferObject()->size();

    float dx = sourceSize.width() / float(targetSize.width());
    int kernelSize;
    auto kernel = createKernel(dx, &kernelSize);
    auto offsets = createOffsets(kernelSize, sourceSize.width(), Qt::Horizontal);


    QOpenGLFramebufferObject scratch(targetSize.width(), sourceSize.height(), framebufferObject()->format());
    scratch.bind();
    glViewport(0, 0, scratch.width(), scratch.height());
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    texture->bind();

    m_program.enableAttributeArray("aPos");
    m_program.enableAttributeArray("aTexCoord");
    m_program.setAttributeArray("aPos",  m_vertices.constData());
    m_program.setAttributeArray("aTexCoord", m_texCoords.constData());

    m_program.setUniformValueArray("kernel", kernel.data(), kernel.size());
    m_program.setUniformValueArray("offsets", offsets.data(), offsets.size());

    glDrawArrays(GL_TRIANGLES, 0, m_vertices.size());
    scratch.bindDefault();

    // restore "real" framebuffer, that the scenegraph will emed
    // only this time our offsets are vertical
    // and we use the scratch framebuffer as the source
    auto frameBuffer = framebufferObject();
    frameBuffer->bind();

    glViewport(0, 0, frameBuffer->width(), frameBuffer->height());
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scratch.texture());

    createOffsets(16, sourceSize.height(), Qt::Vertical);
    m_program.setUniformValueArray("offsets", offsets.data(), offsets.size());

    glDrawArrays(GL_TRIANGLES, 0, m_vertices.size());

    m_program.disableAttributeArray("aPos");
    m_program.disableAttributeArray("aTexCoord");

    update();

    m_program.release();
}

void LanczosRenderer::createGeometry()
{
    m_vertices << QVector2D(-1, -1);
    m_vertices << QVector2D(-1, 1);
    m_vertices << QVector2D(1, -1);
    m_vertices << QVector2D(-1, 1);
    m_vertices << QVector2D(1, -1);
    m_vertices << QVector2D(1, 1);

    m_texCoords << QVector2D(0, 0);
    m_texCoords << QVector2D(0, 1);
    m_texCoords << QVector2D(1, 0);
    m_texCoords << QVector2D(0, 1);
    m_texCoords << QVector2D(1, 0);
    m_texCoords << QVector2D(1, 1);
}

Lanczos::Lanczos()
{
}

QQuickItem * Lanczos::source()
{
    return m_source.data();
}

void Lanczos::setSource(QQuickItem *source)
{
    if (source == m_source) {
        return;
    }
    m_source = source;
    Q_EMIT sourceChanged();
    update();
}

QQuickFramebufferObject::Renderer *Lanczos::createRenderer() const
{
    return new LanczosRenderer();
}

#include "lanczos.moc"
