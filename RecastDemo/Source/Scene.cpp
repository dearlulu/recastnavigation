#include "Scene.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>

#define MAX_SIZE (1 << 6)
#define SCRIPT_COUNT_PER_REGION		16
#define SCRIPT_DATA_SIZE (sizeof(uint32_t) * SCRIPT_COUNT_PER_REGION)

#pragma pack(1)
struct CellBaseInfo
{
    uint32_t		dwCellType			:	2;		// 地表类型
    uint32_t		dwIndoor			:	1;		// 是否室内
    uint32_t		dwPassWidth			:	2;		// 通过物限制
    uint32_t        dwAdvancedObstacle  :   1;      // 高级障碍标记
    uint32_t		dwGradientDirection :   3;		// 滑动方向,8个方向,合并时取最顶的
    uint32_t		dwGradientDegree    :	3;		// 坡度,单位: 90度圆弧的1/8,合并时取最顶上的,超过90度取补角
    uint32_t		dwBarrierDirection  :	3;		// 障碍方向,单位: 180度圆弧的1/8,合并时取倾斜最大的
    uint32_t		dwFaceUp			:	1;		// 是否面向上，用于编辑时合并CELL
    uint32_t		dwDynamic			:	1;		// 是否动态Cell
    
    uint32_t		dwNoObstacleRange	:	6;		// 无障碍范围
    uint32_t		dwScriptIndex		:	4;		// 脚本索引
    uint32_t		dwPutObj			:	1;		// 可摆放
    uint32_t		dwRest				:	1;		// 是否是休息区
    uint32_t        dwSprint            :   1;      // 是否可以爬墙
    uint32_t		dwRideHorse		    :	1;		// 是否下马
    uint32_t		dwBlockCharacter	:	1;		// 障碍信息
};

struct RegionHeader
{
    int32_t Version;
    int32_t RegionX;
    int32_t RegionY;
    int32_t Reserved;
};
#pragma pack()

struct Cell
{
    CellBaseInfo BaseInfo;
    
    int HighLayer;                  // 上表面高度 （相对场景最低点 单位：Layer）
    int LowLayer;                   // 下表面高度 （相对场景最低点 单位：Layer）
    Cell* Next = nullptr;           // 竖直方向向上的下一个Cell (不可交叉重叠)
};

struct Region
{
    int RegionX;
    int RegionY;
    Cell* Cells = nullptr;
    
    Cell* NormalCellArray = nullptr;
    Cell* DynamicCellArray = nullptr;
};



static long GetFileSize(FILE* file)
{
    auto offset = ftell(file);
    fseek(file, 0, SEEK_END);
    auto size = ftell(file);
    fseek(file, offset, SEEK_SET);
    return size;
}

static void ParseVoxelCfg(const char* filePath, int& x, int& y)
{
    const char* keyRegionCountX = "RegionCountX=";
    const char* keyRegionCountY = "RegionCountY=";
    
    auto file = fopen(filePath, "r");
    if (!file)
        return;
    
    char buffer[1024];
    while (std::fgets(buffer, sizeof(buffer), file))
    {
        auto keyX = strstr(buffer, keyRegionCountX);
        if (keyX)
        {
            keyX += strlen(keyRegionCountX);
            x = atol(keyX);
            continue;
        }
        
        auto keyY = strstr(buffer, keyRegionCountY);
        if (keyY)
        {
            keyY += strlen(keyRegionCountY);
            y = atol(keyY);
            break;
        }
    }

    fclose(file);
}

Scene::~Scene()
{
    for (int y = 0; y < m_regionHeight; y++)
    {
        for (int x = 0; x < m_regionWidth; x++)
        {
            Region* region = &m_regions[y * m_regionWidth + x];
            
            if (region->Cells)
                delete[] region->Cells;
            
            if (region->NormalCellArray)
                delete[] region->NormalCellArray;
            
            
            if (region->DynamicCellArray)
                delete[] region->DynamicCellArray;
        }
    }
    
    if (m_regions)
        delete[] m_regions;
}

bool Scene::Load(const char* filePath)
{
    ParseVoxelCfg(filePath, m_regionWidth, m_regionHeight);
    
    assert(m_regionWidth > 0 && m_regionWidth <= MAX_SIZE);
    assert(m_regionHeight > 0 && m_regionHeight <= MAX_SIZE);

    m_regions = new Region[m_regionWidth * m_regionHeight];
    
    for (int y = 0; y < m_regionHeight; y++)
    {
        for (int x = 0; x < m_regionWidth; x++)
        {
            Region* region = &m_regions[y * m_regionWidth + x];
            
            assert(region->Cells == nullptr);
            
            region->RegionX = x;
            region->RegionY = y;
            region->Cells = new Cell[MAX_SIZE * MAX_SIZE];
            LoadRegion(region, filePath);
        }
    }
    
    return true;
}

static Cell* GetLowestObstacle(Region* pRegion, int nXCell, int nYCell)
{
    assert(nXCell >= 0);
    assert(nXCell < MAX_SIZE);
    assert(nYCell >= 0);
    assert(nYCell < MAX_SIZE);
    
    return pRegion->Cells + MAX_SIZE * nYCell + nXCell;
}

static bool AddObstacle(Region* pRegion, int nXCell, int nYCell, Cell* pCell)
{
    auto bFound = false;
    assert(pCell);
    
    auto* pInsertPos = GetLowestObstacle(pRegion, nXCell, nYCell);
    while (!bFound)
    {
        assert(pCell->LowLayer >= pInsertPos->HighLayer);
        
        if (pInsertPos->Next)
        {
            if (pCell->LowLayer >= pInsertPos->HighLayer &&
                pCell->HighLayer <= pInsertPos->Next->LowLayer)
            {
                bFound = true;
            }
            else
            {
                pInsertPos = pInsertPos->Next;
            }
        }
        else
        {			
            bFound = true;
        }
    }
    
    pCell->Next = pInsertPos->Next;
    pInsertPos->Next = pCell;
    
    return true;
}

static bool LoadTerrainBufferV8(Region* pRegion, const uint8_t* pbyData, size_t uDataLen)
{
    bool            bResult                 = false;
    bool            bRetCode                = false;
    size_t          uLeftBytes              = uDataLen;
    const uint8_t*  pbyOffset               = pbyData;
    size_t          uBaseCellInfoSize       = sizeof(CellBaseInfo) + sizeof(uint16_t);
    Cell*           pAllocNormalCell        = NULL;
    Cell*           pAllocDynamicCell       = NULL;
    int             nExtNormalCellCount     = 0;
    size_t          uExtNormalCellDataSize  = sizeof(uint8_t) * 2 + sizeof(CellBaseInfo) + sizeof(uint16_t) * 2;
    int             nExtDynamicCellCount    = 0;
    size_t          uExtDynamicCellDataSize = sizeof(uint8_t) * 2 + sizeof(CellBaseInfo) + sizeof(uint16_t) * 2 + sizeof(uint16_t) * 2;
    
    assert(uLeftBytes >= uBaseCellInfoSize * MAX_SIZE * MAX_SIZE);
    uLeftBytes -= uBaseCellInfoSize * MAX_SIZE * MAX_SIZE;
    
    for (int nCellY = 0; nCellY < MAX_SIZE; nCellY++)
    {
        for (int nCellX = 0; nCellX < MAX_SIZE; nCellX++)
        {
            Cell*               pCell       = GetLowestObstacle(pRegion, nCellX, nCellY);
            CellBaseInfo*       pBaseInfo   = (CellBaseInfo*)pbyOffset;
            
            pCell->BaseInfo           = *pBaseInfo;
            pCell->BaseInfo.dwDynamic = 0;
            pCell->LowLayer           = 0;
            
            pbyOffset += sizeof(CellBaseInfo);
            
            pCell->HighLayer = *(uint16_t*)pbyOffset;
            pbyOffset += sizeof(uint16_t);
        }
    }
    
    assert(uLeftBytes >= sizeof(int32_t));
    uLeftBytes -= sizeof(int32_t);
    
    nExtNormalCellCount = *(int32_t*)pbyOffset;
    pbyOffset += sizeof(int32_t);
    
    assert(nExtNormalCellCount >= 0);
    assert(uLeftBytes >= nExtNormalCellCount * uExtNormalCellDataSize);
    uLeftBytes -= nExtNormalCellCount * uExtNormalCellDataSize;
    
    if (nExtNormalCellCount > 0)
    {
        assert(pRegion->NormalCellArray == nullptr);
        pRegion->NormalCellArray = new Cell[nExtNormalCellCount];
    }
    
    for (int nIndex = 0; nIndex < nExtNormalCellCount; nIndex++)
    {
        int                 nCellX      = 0;
        int                 nCellY      = 0;
        CellBaseInfo*       pBaseInfo   = NULL;
        
        pAllocNormalCell = pRegion->NormalCellArray + nIndex;
        
        nCellX = *(uint8_t*)pbyOffset;
        pbyOffset += sizeof(uint8_t);
        
        nCellY = *(uint8_t*)pbyOffset;
        pbyOffset += sizeof(uint8_t);
        
        pBaseInfo = (CellBaseInfo*)pbyOffset;
        pbyOffset += sizeof(CellBaseInfo);
        
        pAllocNormalCell->BaseInfo = *pBaseInfo;
        pAllocNormalCell->BaseInfo.dwDynamic = 0;
        
        pAllocNormalCell->HighLayer = *(uint16_t*)pbyOffset;
        pbyOffset += sizeof(uint16_t);
        
        pAllocNormalCell->LowLayer = *(uint16_t*)pbyOffset;
        pbyOffset += sizeof(uint16_t);
        
        bRetCode = AddObstacle(pRegion, nCellX, nCellY, pAllocNormalCell);
        assert(bRetCode);
        
        pAllocNormalCell = NULL;
    }
    
    assert(uLeftBytes >= sizeof(int32_t));
    uLeftBytes -= sizeof(int32_t);
    
    nExtDynamicCellCount = *(int32_t*)pbyOffset;
    pbyOffset += sizeof(int32_t);
    
    assert(nExtDynamicCellCount >= 0);
    assert(uLeftBytes >= nExtDynamicCellCount * uExtDynamicCellDataSize);
    uLeftBytes -= nExtDynamicCellCount * uExtDynamicCellDataSize;
    
    if (nExtDynamicCellCount > 0)
    {
        assert(pRegion->DynamicCellArray == nullptr);
        pRegion->DynamicCellArray = new Cell[nExtDynamicCellCount];
    }
    
    for (int nIndex = 0; nIndex < nExtDynamicCellCount; nIndex++)
    {
        int                 nCellX      = 0;
        int                 nCellY      = 0;
        CellBaseInfo*       pBaseInfo   = NULL;
        
        pAllocDynamicCell = pRegion->DynamicCellArray + nIndex;
        assert(pAllocDynamicCell);
        
        nCellX = *(uint8_t*)pbyOffset;
        pbyOffset += sizeof(uint8_t);
        
        nCellY = *(uint8_t*)pbyOffset;
        pbyOffset += sizeof(uint8_t);
        
        pBaseInfo = (CellBaseInfo*)pbyOffset;
        pbyOffset += sizeof(CellBaseInfo);
        
        pAllocDynamicCell->BaseInfo = *pBaseInfo;
        pAllocDynamicCell->BaseInfo.dwDynamic = 1;
        
        pAllocDynamicCell->HighLayer = *(uint16_t*)pbyOffset;
        pbyOffset += sizeof(uint16_t);
        
        pAllocDynamicCell->LowLayer = *(uint16_t*)pbyOffset;
        pbyOffset += sizeof(uint16_t);
        
        pbyOffset += sizeof(uint16_t);
        pbyOffset += sizeof(uint16_t);
        
        bRetCode = AddObstacle(pRegion, nCellX, nCellY, pAllocDynamicCell);
        assert(bRetCode);
        
        pAllocDynamicCell = NULL;
    }
    
    
    if (uLeftBytes >= SCRIPT_DATA_SIZE)
    {
        pbyOffset  += SCRIPT_DATA_SIZE;
        uLeftBytes -= SCRIPT_DATA_SIZE;
    }
    
    assert(uLeftBytes == 0);
    
    bResult = true;
    return bResult;
}

bool Scene::LoadRegion(Region* region, const char* fileFolder)
{
    char filePath[1024];
    snprintf(filePath, sizeof(filePath), "%s.data/v_%03d/%03d_Region.map", fileFolder, region->RegionY, region->RegionX);
    
    auto file = fopen(filePath, "rb");
    auto fileSize = GetFileSize(file);
    
    auto buffer = new uint8_t[fileSize];
    fread(buffer, fileSize, 1, file);
    
    auto header = (RegionHeader*)buffer;
    assert(header->RegionX == region->RegionX);
    assert(header->RegionY == region->RegionY);
    assert(header->Version == 8);
    
    LoadTerrainBufferV8(region, buffer + sizeof(RegionHeader), fileSize - sizeof(RegionHeader));
    return true;
}
