#pragma once
#include "Recast.h"

struct Region;
class Scene
{
public:
    ~Scene();
    bool Load(const char* filePath);
    bool SetConfig(rcConfig* hf);
    bool RasterizeScene(rcContext* ctx, rcHeightfield* hf, const int flagMergeThr);
 
private:
    bool LoadRegion(Region* region, const char* filePath);
    int GetSceneHeight();
    
private:
    int m_regionWidth = 0;
    int m_regionHeight = 0;
    Region* m_regions = nullptr;
};
