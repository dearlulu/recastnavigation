#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "SDL.h"
#include "SDL_opengl.h"
#include "imgui.h"
#include "InputGeom.h"
#include "Sample.h"
#include "Sample_Voxels.h"
#include "Recast.h"
#include "RecastDebugDraw.h"
#include "RecastDump.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourDebugDraw.h"
#include "NavMeshTesterTool.h"
#include "NavMeshPruneTool.h"
#include "OffMeshConnectionTool.h"
#include "ConvexVolumeTool.h"
#include "CrowdTool.h"
#include "Filelist.h"
#include "imgui.h"

#ifdef WIN32
#	define snprintf _snprintf
#endif


Sample_Voxels::Sample_Voxels()
{
	setTool(new NavMeshTesterTool);
}
		
Sample_Voxels::~Sample_Voxels()
{
    delete m_scene;
    m_scene = nullptr;
    
	cleanup();
}
	
void Sample_Voxels::cleanup()
{
	rcFreeHeightField(m_solid);
	m_solid = 0;
	rcFreeCompactHeightfield(m_chf);
	m_chf = 0;
	rcFreeContourSet(m_cset);
	m_cset = 0;
	rcFreePolyMesh(m_pmesh);
	m_pmesh = 0;
	rcFreePolyMeshDetail(m_dmesh);
	m_dmesh = 0;
	dtFreeNavMesh(m_navMesh);
	m_navMesh = 0;
}
			
void Sample_Voxels::handleSettings()
{
	Sample::handleCommonSettings();
	
	if (imguiCheck("Keep Itermediate Results", m_keepInterResults))
		m_keepInterResults = !m_keepInterResults;

	imguiSeparator();
	
	char msg[64];
	snprintf(msg, 64, "Build Time: %.1fms", m_totalBuildTimeMs);
	imguiLabel(msg);
	
	imguiSeparator();
}

void Sample_Voxels::handleTools()
{
	int type = !m_tool ? TOOL_NONE : m_tool->type();
	
	if (imguiCheck("Test Navmesh", type == TOOL_NAVMESH_TESTER))
	{
		setTool(new NavMeshTesterTool);
	}
	if (imguiCheck("Prune Navmesh", type == TOOL_NAVMESH_PRUNE))
	{
		setTool(new NavMeshPruneTool);
	}
	if (imguiCheck("Create Off-Mesh Connections", type == TOOL_OFFMESH_CONNECTION))
	{
		setTool(new OffMeshConnectionTool);
	}
	if (imguiCheck("Create Convex Volumes", type == TOOL_CONVEX_VOLUME))
	{
		setTool(new ConvexVolumeTool);
	}
	if (imguiCheck("Create Crowds", type == TOOL_CROWD))
	{
		setTool(new CrowdTool);
	}
	
	imguiSeparatorLine();

	imguiIndent();

	if (m_tool)
		m_tool->handleMenu();

	imguiUnindent();

}

void Sample_Voxels::handleVoxelFile(const std::string& filePath)
{
    auto scene = new Scene;
    if (!scene->Load(filePath.c_str()))
    {
        delete scene;
        return;
    }
    
    if (m_scene)
        delete m_scene;
    m_scene = scene;
}

void Sample_Voxels::selectVoxelFile()
{
    const std::string voxelsFolder = "Voxels";
    static std::vector<std::string> files;
    
    imguiSeparator();
    imguiLabel("Input Voxels");
    if (imguiButton(m_voxelsName.c_str()))
    {
        if (m_showScenes)
        {
            m_showScenes = false;
        }
        else
        {
            m_showScenes = true;
            scanDirectory(voxelsFolder, ".cfg", files);
        }
    }
    
    if (m_showScenes)
    {
        std::string levelToLoad;
        for(auto& file : files)
        {
            if (imguiItem(file.c_str()))
            {
                levelToLoad = file;
            }
        }
        
        if (!levelToLoad.empty())
        {
            m_showScenes = false;
            
            std::string path = voxelsFolder + "/" + levelToLoad;
            handleVoxelFile(path);
            m_voxelsName = levelToLoad;
        }
    }
}

void Sample_Voxels::handleDebugMode()
{
    selectVoxelFile();
    if (!m_scene)
        return;

	// Check which modes are valid.
	bool valid[MAX_DRAWMODE];
    memset(valid, 0, sizeof(valid));
    
    valid[DRAWMODE_NAVMESH] = m_navMesh != 0;
    valid[DRAWMODE_NAVMESH_TRANS] = m_navMesh != 0;
    valid[DRAWMODE_NAVMESH_BVTREE] = m_navMesh != 0;
    valid[DRAWMODE_NAVMESH_NODES] = m_navQuery != 0;
    valid[DRAWMODE_NAVMESH_INVIS] = m_navMesh != 0;
    valid[DRAWMODE_MESH] = true;
    valid[DRAWMODE_VOXELS] = m_solid != 0;
    valid[DRAWMODE_VOXELS_WALKABLE] = m_solid != 0;
    valid[DRAWMODE_COMPACT] = m_chf != 0;
    valid[DRAWMODE_COMPACT_DISTANCE] = m_chf != 0;
    valid[DRAWMODE_COMPACT_REGIONS] = m_chf != 0;
    valid[DRAWMODE_REGION_CONNECTIONS] = m_cset != 0;
    valid[DRAWMODE_RAW_CONTOURS] = m_cset != 0;
    valid[DRAWMODE_BOTH_CONTOURS] = m_cset != 0;
    valid[DRAWMODE_CONTOURS] = m_cset != 0;
    valid[DRAWMODE_POLYMESH] = m_pmesh != 0;
    valid[DRAWMODE_POLYMESH_DETAIL] = m_dmesh != 0;

	imguiLabel("Draw");
	if (imguiCheck("Input Mesh", m_drawMode == DRAWMODE_MESH, valid[DRAWMODE_MESH]))
		m_drawMode = DRAWMODE_MESH;
	if (imguiCheck("Navmesh", m_drawMode == DRAWMODE_NAVMESH, valid[DRAWMODE_NAVMESH]))
		m_drawMode = DRAWMODE_NAVMESH;
	if (imguiCheck("Navmesh Invis", m_drawMode == DRAWMODE_NAVMESH_INVIS, valid[DRAWMODE_NAVMESH_INVIS]))
		m_drawMode = DRAWMODE_NAVMESH_INVIS;
	if (imguiCheck("Navmesh Trans", m_drawMode == DRAWMODE_NAVMESH_TRANS, valid[DRAWMODE_NAVMESH_TRANS]))
		m_drawMode = DRAWMODE_NAVMESH_TRANS;
	if (imguiCheck("Navmesh BVTree", m_drawMode == DRAWMODE_NAVMESH_BVTREE, valid[DRAWMODE_NAVMESH_BVTREE]))
		m_drawMode = DRAWMODE_NAVMESH_BVTREE;
	if (imguiCheck("Navmesh Nodes", m_drawMode == DRAWMODE_NAVMESH_NODES, valid[DRAWMODE_NAVMESH_NODES]))
		m_drawMode = DRAWMODE_NAVMESH_NODES;
	if (imguiCheck("Voxels", m_drawMode == DRAWMODE_VOXELS, valid[DRAWMODE_VOXELS]))
		m_drawMode = DRAWMODE_VOXELS;
	if (imguiCheck("Walkable Voxels", m_drawMode == DRAWMODE_VOXELS_WALKABLE, valid[DRAWMODE_VOXELS_WALKABLE]))
		m_drawMode = DRAWMODE_VOXELS_WALKABLE;
	if (imguiCheck("Compact", m_drawMode == DRAWMODE_COMPACT, valid[DRAWMODE_COMPACT]))
		m_drawMode = DRAWMODE_COMPACT;
	if (imguiCheck("Compact Distance", m_drawMode == DRAWMODE_COMPACT_DISTANCE, valid[DRAWMODE_COMPACT_DISTANCE]))
		m_drawMode = DRAWMODE_COMPACT_DISTANCE;
	if (imguiCheck("Compact Regions", m_drawMode == DRAWMODE_COMPACT_REGIONS, valid[DRAWMODE_COMPACT_REGIONS]))
		m_drawMode = DRAWMODE_COMPACT_REGIONS;
	if (imguiCheck("Region Connections", m_drawMode == DRAWMODE_REGION_CONNECTIONS, valid[DRAWMODE_REGION_CONNECTIONS]))
		m_drawMode = DRAWMODE_REGION_CONNECTIONS;
	if (imguiCheck("Raw Contours", m_drawMode == DRAWMODE_RAW_CONTOURS, valid[DRAWMODE_RAW_CONTOURS]))
		m_drawMode = DRAWMODE_RAW_CONTOURS;
	if (imguiCheck("Both Contours", m_drawMode == DRAWMODE_BOTH_CONTOURS, valid[DRAWMODE_BOTH_CONTOURS]))
		m_drawMode = DRAWMODE_BOTH_CONTOURS;
	if (imguiCheck("Contours", m_drawMode == DRAWMODE_CONTOURS, valid[DRAWMODE_CONTOURS]))
		m_drawMode = DRAWMODE_CONTOURS;
	if (imguiCheck("Poly Mesh", m_drawMode == DRAWMODE_POLYMESH, valid[DRAWMODE_POLYMESH]))
		m_drawMode = DRAWMODE_POLYMESH;
	if (imguiCheck("Poly Mesh Detail", m_drawMode == DRAWMODE_POLYMESH_DETAIL, valid[DRAWMODE_POLYMESH_DETAIL]))
		m_drawMode = DRAWMODE_POLYMESH_DETAIL;
}

void Sample_Voxels::handleRender()
{
	if (!m_solid)
		return;
	
	DebugDrawGL dd;
	
	glDisable(GL_FOG);
	glDepthMask(GL_FALSE);

	// Draw bounds
	const float* bmin = m_solid->bmin;
	const float* bmax = m_solid->bmax;
	duDebugDrawBoxWire(&dd, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], duRGBA(255,255,255,128), 1.0f);
	dd.begin(DU_DRAW_POINTS, 5.0f);
	dd.vertex(bmin[0],bmin[1],bmin[2],duRGBA(255,255,255,128));
	dd.end();
	
	if (m_navMesh && m_navQuery &&
		(m_drawMode == DRAWMODE_NAVMESH ||
		m_drawMode == DRAWMODE_NAVMESH_TRANS ||
		m_drawMode == DRAWMODE_NAVMESH_BVTREE ||
		 m_drawMode == DRAWMODE_NAVMESH_NODES ||
		m_drawMode == DRAWMODE_NAVMESH_INVIS))
	{
		if (m_drawMode != DRAWMODE_NAVMESH_INVIS)
			duDebugDrawNavMeshWithClosedList(&dd, *m_navMesh, *m_navQuery, m_navMeshDrawFlags);
		if (m_drawMode == DRAWMODE_NAVMESH_BVTREE)
			duDebugDrawNavMeshBVTree(&dd, *m_navMesh);
		if (m_drawMode == DRAWMODE_NAVMESH_NODES)
			duDebugDrawNavMeshNodes(&dd, *m_navQuery);
		duDebugDrawNavMeshPolysWithFlags(&dd, *m_navMesh, SAMPLE_POLYFLAGS_DISABLED, duRGBA(0,0,0,128));
	}
		
	glDepthMask(GL_TRUE);
	
	if (m_chf && m_drawMode == DRAWMODE_COMPACT)
		duDebugDrawCompactHeightfieldSolid(&dd, *m_chf);

	if (m_chf && m_drawMode == DRAWMODE_COMPACT_DISTANCE)
		duDebugDrawCompactHeightfieldDistance(&dd, *m_chf);
	if (m_chf && m_drawMode == DRAWMODE_COMPACT_REGIONS)
		duDebugDrawCompactHeightfieldRegions(&dd, *m_chf);
	if (m_solid && m_drawMode == DRAWMODE_VOXELS)
	{
		glEnable(GL_FOG);
		duDebugDrawHeightfieldSolid(&dd, *m_solid);
		glDisable(GL_FOG);
	}
	if (m_solid && m_drawMode == DRAWMODE_VOXELS_WALKABLE)
	{
		glEnable(GL_FOG);
		duDebugDrawHeightfieldWalkable(&dd, *m_solid);
		glDisable(GL_FOG);
	}
	if (m_cset && m_drawMode == DRAWMODE_RAW_CONTOURS)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawRawContours(&dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	if (m_cset && m_drawMode == DRAWMODE_BOTH_CONTOURS)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawRawContours(&dd, *m_cset, 0.5f);
		duDebugDrawContours(&dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	if (m_cset && m_drawMode == DRAWMODE_CONTOURS)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawContours(&dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	if (m_chf && m_cset && m_drawMode == DRAWMODE_REGION_CONNECTIONS)
	{
		duDebugDrawCompactHeightfieldRegions(&dd, *m_chf);
			
		glDepthMask(GL_FALSE);
		duDebugDrawRegionConnections(&dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	if (m_pmesh && m_drawMode == DRAWMODE_POLYMESH)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawPolyMesh(&dd, *m_pmesh);
		glDepthMask(GL_TRUE);
	}
	if (m_dmesh && m_drawMode == DRAWMODE_POLYMESH_DETAIL)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawPolyMeshDetail(&dd, *m_dmesh);
		glDepthMask(GL_TRUE);
	}
	
	if (m_tool)
		m_tool->handleRender();
	renderToolStates();

	glDepthMask(GL_TRUE);
}

void Sample_Voxels::handleRenderOverlay(double* proj, double* model, int* view)
{
	if (m_tool)
		m_tool->handleRenderOverlay(proj, model, view);
	renderOverlayToolStates(proj, model, view);
}

void Sample_Voxels::handleMeshChanged(class InputGeom* geom)
{
	Sample::handleMeshChanged(geom);

	dtFreeNavMesh(m_navMesh);
	m_navMesh = 0;

	if (m_tool)
	{
		m_tool->reset();
		m_tool->init(this);
	}
	resetToolStates();
	initToolStates(this);
}


bool Sample_Voxels::handleBuild()
{
	if (!m_scene)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Input mesh is not specified.");
		return false;
	}
	
	cleanup();
    
    //
    // Step 1. Initialize build config.
    //
    
    // Init build configuration from GUI
    memset(&m_cfg, 0, sizeof(m_cfg));
    m_scene->SetConfig(&m_cfg);
    
    m_cfg.walkableSlopeAngle = m_agentMaxSlope;
    m_cfg.walkableHeight = (int)ceilf(m_agentHeight / m_cfg.ch);
    m_cfg.walkableClimb = (int)floorf(m_agentMaxClimb / m_cfg.ch);
    m_cfg.walkableRadius = (int)ceilf(m_agentRadius / m_cfg.cs);
    m_cfg.maxEdgeLen = (int)(m_edgeMaxLen / m_cfg.cs);
    m_cfg.maxSimplificationError = m_edgeMaxError;
    m_cfg.minRegionArea = (int)rcSqr(m_regionMinSize);		// Note: area = size*size
    m_cfg.mergeRegionArea = (int)rcSqr(m_regionMergeSize);	// Note: area = size*size
    m_cfg.maxVertsPerPoly = (int)m_vertsPerPoly;
    m_cfg.detailSampleDist = m_detailSampleDist < 0.9f ? 0 : m_cfg.cs * m_detailSampleDist;
    m_cfg.detailSampleMaxError = m_cfg.ch * m_detailSampleMaxError;
    
    // Reset build times gathering.
    m_ctx->resetTimers();
    
    // Start the build process.
    m_ctx->startTimer(RC_TIMER_TOTAL);
    
    m_ctx->log(RC_LOG_PROGRESS, "Building navigation:");
    m_ctx->log(RC_LOG_PROGRESS, " - %d x %d cells", m_cfg.width, m_cfg.height);
    
    //
    // Step 2. Rasterize input polygon soup.
    //
    
    // Allocate voxel heightfield where we rasterize our input data to.
    m_solid = rcAllocHeightfield();
    if (!m_solid)
    {
        m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'solid'.");
        return false;
    }
    if (!rcCreateHeightfield(m_ctx, *m_solid, m_cfg.width, m_cfg.height, m_cfg.bmin, m_cfg.bmax, m_cfg.cs, m_cfg.ch))
    {
        m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not create solid heightfield.");
        return false;
    }
    
    if (!m_scene->RasterizeScene(m_ctx, m_solid, m_cfg.walkableClimb))
    {
        m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not rasterize scene.");
        return false;
    }
    
    //
    // Step 3. Filter walkables surfaces.
    //
    
    // Once all geoemtry is rasterized, we do initial pass of filtering to
    // remove unwanted overhangs caused by the conservative rasterization
    // as well as filter spans where the character cannot possibly stand.

    rcFilterLowHangingWalkableObstacles(m_ctx, m_cfg.walkableClimb, *m_solid);
    rcFilterLedgeSpans(m_ctx, m_cfg.walkableHeight, m_cfg.walkableClimb, *m_solid);
    rcFilterWalkableLowHeightSpans(m_ctx, m_cfg.walkableHeight, *m_solid);
    
    //
    // Step 4. Partition walkable surface to simple regions.
    //
    
    // Compact the heightfield so that it is faster to handle from now on.
    // This will result more cache coherent data as well as the neighbours
    // between walkable cells will be calculated.
    m_chf = rcAllocCompactHeightfield();
    if (!m_chf)
    {
        m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'chf'.");
        return false;
    }
    if (!rcBuildCompactHeightfield(m_ctx, m_cfg.walkableHeight, m_cfg.walkableClimb, *m_solid, *m_chf))
    {
        m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build compact data.");
        return false;
    }
    
    if (!m_keepInterResults)
    {
        rcFreeHeightField(m_solid);
        m_solid = 0;
    }
    
    // Erode the walkable area by agent radius.
    if (!rcErodeWalkableArea(m_ctx, m_cfg.walkableRadius, *m_chf))
    {
        m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not erode.");
        return false;
    }
    
    
    // Partition the heightfield so that we can use simple algorithm later to triangulate the walkable areas.
    // There are 3 martitioning methods, each with some pros and cons:
    // 1) Watershed partitioning
    //   - the classic Recast partitioning
    //   - creates the nicest tessellation
    //   - usually slowest
    //   - partitions the heightfield into nice regions without holes or overlaps
    //   - the are some corner cases where this method creates produces holes and overlaps
    //      - holes may appear when a small obstacles is close to large open area (triangulation can handle this)
    //      - overlaps may occur if you have narrow spiral corridors (i.e stairs), this make triangulation to fail
    //   * generally the best choice if you precompute the nacmesh, use this if you have large open areas
    // 2) Monotone partioning
    //   - fastest
    //   - partitions the heightfield into regions without holes and overlaps (guaranteed)
    //   - creates long thin polygons, which sometimes causes paths with detours
    //   * use this if you want fast navmesh generation
    // 3) Layer partitoining
    //   - quite fast
    //   - partitions the heighfield into non-overlapping regions
    //   - relies on the triangulation code to cope with holes (thus slower than monotone partitioning)
    //   - produces better triangles than monotone partitioning
    //   - does not have the corner cases of watershed partitioning
    //   - can be slow and create a bit ugly tessellation (still better than monotone)
    //     if you have large open areas with small obstacles (not a problem if you use tiles)
    //   * good choice to use for tiled navmesh with medium and small sized tiles
    
    if (m_partitionType == SAMPLE_PARTITION_WATERSHED)
    {
        // Prepare for region partitioning, by calculating distance field along the walkable surface.
        if (!rcBuildDistanceField(m_ctx, *m_chf))
        {
            m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build distance field.");
            return false;
        }
        
        // Partition the walkable surface into simple regions without holes.
        if (!rcBuildRegions(m_ctx, *m_chf, 0, m_cfg.minRegionArea, m_cfg.mergeRegionArea))
        {
            m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build watershed regions.");
            return false;
        }
    }
    else if (m_partitionType == SAMPLE_PARTITION_MONOTONE)
    {
        // Partition the walkable surface into simple regions without holes.
        // Monotone partitioning does not need distancefield.
        if (!rcBuildRegionsMonotone(m_ctx, *m_chf, 0, m_cfg.minRegionArea, m_cfg.mergeRegionArea))
        {
            m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build monotone regions.");
            return false;
        }
    }
    else // SAMPLE_PARTITION_LAYERS
    {
        // Partition the walkable surface into simple regions without holes.
        if (!rcBuildLayerRegions(m_ctx, *m_chf, 0, m_cfg.minRegionArea))
        {
            m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build layer regions.");
            return false;
        }
    }
	
	//
	// Step 5. Trace and simplify region contours.
	//
	
	// Create contours.
	m_cset = rcAllocContourSet();
	if (!m_cset)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'cset'.");
		return false;
	}
	if (!rcBuildContours(m_ctx, *m_chf, m_cfg.maxSimplificationError, m_cfg.maxEdgeLen, *m_cset))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not create contours.");
		return false;
	}
	
	//
	// Step 6. Build polygons mesh from contours.
	//
	
	// Build polygon navmesh from the contours.
	m_pmesh = rcAllocPolyMesh();
	if (!m_pmesh)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'pmesh'.");
		return false;
	}
	if (!rcBuildPolyMesh(m_ctx, *m_cset, m_cfg.maxVertsPerPoly, *m_pmesh))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not triangulate contours.");
		return false;
	}
	
	//
	// Step 7. Create detail mesh which allows to access approximate height on each polygon.
	//
	
	m_dmesh = rcAllocPolyMeshDetail();
	if (!m_dmesh)
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'pmdtl'.");
		return false;
	}

	if (!rcBuildPolyMeshDetail(m_ctx, *m_pmesh, *m_chf, m_cfg.detailSampleDist, m_cfg.detailSampleMaxError, *m_dmesh))
	{
		m_ctx->log(RC_LOG_ERROR, "buildNavigation: Could not build detail mesh.");
		return false;
	}

	if (!m_keepInterResults)
	{
		rcFreeCompactHeightfield(m_chf);
		m_chf = 0;
		rcFreeContourSet(m_cset);
		m_cset = 0;
	}

	// At this point the navigation mesh data is ready, you can access it from m_pmesh.
	// See duDebugDrawPolyMesh or dtCreateNavMeshData as examples how to access the data.
	
	//
	// (Optional) Step 8. Create Detour data from Recast poly mesh.
	//
	
	// The GUI may allow more max points per polygon than Detour can handle.
	// Only build the detour navmesh if we do not exceed the limit.
	if (m_cfg.maxVertsPerPoly <= DT_VERTS_PER_POLYGON)
	{
		unsigned char* navData = 0;
		int navDataSize = 0;

		// Update poly flags from areas.
		for (int i = 0; i < m_pmesh->npolys; ++i)
		{
			if (m_pmesh->areas[i] == RC_WALKABLE_AREA)
				m_pmesh->areas[i] = SAMPLE_POLYAREA_GROUND;
				
			if (m_pmesh->areas[i] == SAMPLE_POLYAREA_GROUND ||
				m_pmesh->areas[i] == SAMPLE_POLYAREA_GRASS ||
				m_pmesh->areas[i] == SAMPLE_POLYAREA_ROAD)
			{
				m_pmesh->flags[i] = SAMPLE_POLYFLAGS_WALK;
			}
			else if (m_pmesh->areas[i] == SAMPLE_POLYAREA_WATER)
			{
				m_pmesh->flags[i] = SAMPLE_POLYFLAGS_SWIM;
			}
			else if (m_pmesh->areas[i] == SAMPLE_POLYAREA_DOOR)
			{
				m_pmesh->flags[i] = SAMPLE_POLYFLAGS_WALK | SAMPLE_POLYFLAGS_DOOR;
			}
		}


		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = m_pmesh->verts;
		params.vertCount = m_pmesh->nverts;
		params.polys = m_pmesh->polys;
		params.polyAreas = m_pmesh->areas;
		params.polyFlags = m_pmesh->flags;
		params.polyCount = m_pmesh->npolys;
		params.nvp = m_pmesh->nvp;
		params.detailMeshes = m_dmesh->meshes;
		params.detailVerts = m_dmesh->verts;
		params.detailVertsCount = m_dmesh->nverts;
		params.detailTris = m_dmesh->tris;
		params.detailTriCount = m_dmesh->ntris;
        /*
		params.offMeshConVerts = m_geom->getOffMeshConnectionVerts();
		params.offMeshConRad = m_geom->getOffMeshConnectionRads();
		params.offMeshConDir = m_geom->getOffMeshConnectionDirs();
		params.offMeshConAreas = m_geom->getOffMeshConnectionAreas();
		params.offMeshConFlags = m_geom->getOffMeshConnectionFlags();
		params.offMeshConUserID = m_geom->getOffMeshConnectionId();
		params.offMeshConCount = m_geom->getOffMeshConnectionCount();
         */
		params.walkableHeight = m_agentHeight;
		params.walkableRadius = m_agentRadius;
		params.walkableClimb = m_agentMaxClimb;
		rcVcopy(params.bmin, m_pmesh->bmin);
		rcVcopy(params.bmax, m_pmesh->bmax);
		params.cs = m_cfg.cs;
		params.ch = m_cfg.ch;
		params.buildBvTree = true;
		
		if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
		{
			m_ctx->log(RC_LOG_ERROR, "Could not build Detour navmesh.");
			return false;
		}
		
		m_navMesh = dtAllocNavMesh();
		if (!m_navMesh)
		{
			dtFree(navData);
			m_ctx->log(RC_LOG_ERROR, "Could not create Detour navmesh");
			return false;
		}
		
		dtStatus status;
		
		status = m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
		if (dtStatusFailed(status))
		{
			dtFree(navData);
			m_ctx->log(RC_LOG_ERROR, "Could not init Detour navmesh");
			return false;
		}
		
		status = m_navQuery->init(m_navMesh, 2048);
		if (dtStatusFailed(status))
		{
			m_ctx->log(RC_LOG_ERROR, "Could not init Detour navmesh query");
			return false;
		}
	}
	
	m_ctx->stopTimer(RC_TIMER_TOTAL);

	// Show performance stats.
	duLogBuildTimes(*m_ctx, m_ctx->getAccumulatedTime(RC_TIMER_TOTAL));
	m_ctx->log(RC_LOG_PROGRESS, ">> Polymesh: %d vertices  %d polygons", m_pmesh->nverts, m_pmesh->npolys);
	
	m_totalBuildTimeMs = m_ctx->getAccumulatedTime(RC_TIMER_TOTAL)/1000.0f;
	
	if (m_tool)
		m_tool->init(this);
	initToolStates(this);

	return true;
}
