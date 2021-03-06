#include "texturecachemanager.h"

TextureCacheManager::TextureCacheManager()
    : m_currentId(0)
{
    m_caches.clear();
}

TextureCacheManager::~TextureCacheManager()
{
    for (int i = 0; i < m_caches.size(); i++)
    {
        if (m_caches[i].second != NULL)
        {
            delete m_caches[i].second;
        }
    }
    m_caches.clear();
}

GLuint TextureCacheManager::add(QOpenGLTexture *pTex)
{
    for (int i = 0; i < m_caches.size(); i++)
    {
        if (m_caches[i].second == pTex)
        {
            return m_caches[i].first;
        }
    }

    m_currentId++;
    m_caches << qMakePair(m_currentId, pTex);
    return m_currentId;
}

void TextureCacheManager::remove(GLuint id)
{
    int removeIndex = -1;
    for (int i = 0; i < m_caches.size(); i++)
    {
        if (m_caches[i].first == id)
        {
            removeIndex = i;
            break;
        }
    }
    if (removeIndex < 0)
    {
        return;
    }
    delete m_caches[removeIndex].second;
    m_caches.removeAt(removeIndex);
}

QOpenGLTexture *TextureCacheManager::get(GLuint id)
{
    for (int i = 0; i < m_caches.size(); i++)
    {
        if (m_caches[i].first == id)
        {
            return m_caches[i].second;
        }
    }
    return NULL;
}
