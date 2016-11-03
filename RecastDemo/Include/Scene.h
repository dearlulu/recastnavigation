#pragma once

struct Region;
class Scene
{
public:
    ~Scene();
    bool Load(const char* filePath);
 
private:
    bool LoadRegion(Region* region, const char* filePath);
    
private:
    int m_regionWidth = 0;
    int m_regionHeight = 0;
    Region* m_regions = nullptr;
};
