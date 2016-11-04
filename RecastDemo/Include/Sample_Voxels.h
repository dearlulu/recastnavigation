#pragma once

#include "Sample.h"
#include "DetourNavMesh.h"
#include "Recast.h"
#include "Scene.h"

class Sample_Voxels : public Sample
{
protected:
    Scene* m_scene = nullptr;
    std::string m_voxelsName;
    bool m_showScenes = false;
    
	bool m_keepInterResults = true;
	float m_totalBuildTimeMs = 0;
	rcHeightfield* m_solid = nullptr;
	rcCompactHeightfield* m_chf = nullptr;
	rcContourSet* m_cset = nullptr;
	rcPolyMesh* m_pmesh = nullptr;
	rcConfig m_cfg;	
	rcPolyMeshDetail* m_dmesh = nullptr;
    
	enum DrawMode
	{
		DRAWMODE_NAVMESH,
		DRAWMODE_NAVMESH_TRANS,
		DRAWMODE_NAVMESH_BVTREE,
		DRAWMODE_NAVMESH_NODES,
		DRAWMODE_NAVMESH_INVIS,
		DRAWMODE_MESH,
		DRAWMODE_VOXELS,
		DRAWMODE_VOXELS_WALKABLE,
		DRAWMODE_COMPACT,
		DRAWMODE_COMPACT_DISTANCE,
		DRAWMODE_COMPACT_REGIONS,
		DRAWMODE_REGION_CONNECTIONS,
		DRAWMODE_RAW_CONTOURS,
		DRAWMODE_BOTH_CONTOURS,
		DRAWMODE_CONTOURS,
		DRAWMODE_POLYMESH,
		DRAWMODE_POLYMESH_DETAIL,
		MAX_DRAWMODE
	};
	
	DrawMode m_drawMode = DRAWMODE_NAVMESH;
	
	void cleanup();
    void selectVoxelFile();
    void handleVoxelFile(const std::string& filePath);
		
public:
	Sample_Voxels();
	virtual ~Sample_Voxels();
	
	virtual void handleSettings();
	virtual void handleTools();
	virtual void handleDebugMode();
	
	virtual void handleRender();
	virtual void handleRenderOverlay(double* proj, double* model, int* view);
	virtual void handleMeshChanged(class InputGeom* geom);
	virtual bool handleBuild();

private:
	// Explicitly disabled copy constructor and copy assignment operator.
	Sample_Voxels(const Sample_Voxels&);
	Sample_Voxels& operator=(const Sample_Voxels&);
};


