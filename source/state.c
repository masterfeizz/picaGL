#include "internal.h"
#include "vshader_shbin.h"

pgl_state_t pgl_state;

static const size_t  render_target_bpp[5] = { 4, 3, 2, 2, 2 };
static const GPU_COLORBUF fb_to_rt_fmt[5] = { 0, 1, 3, 2, 4 };

static inline void pgl_texenv_reset(pgl_texenv_t* env)
{
	env->src_rgb     = GPU_TEVSOURCES(GPU_PREVIOUS, 0, 0);
	env->src_alpha   = env->src_rgb;
	env->op_rgb      = GPU_TEVOPERANDS(0,0,0);
	env->op_alpha    = env->op_rgb;
	env->func_rgb    = GPU_REPLACE;
	env->func_alpha  = env->func_rgb;
	env->color       = 0xFFFFFFFF;
	env->scale_rgb   = GPU_TEVSCALE_1;
	env->scale_alpha = GPU_TEVSCALE_1;
}

void pgl_state_initialize()
{
	pgl_state.command_buffer = linearAlloc(COMMAND_BUFFER_SIZE);

	GPUCMD_SetBuffer(pgl_state.command_buffer, COMMAND_BUFFER_LENGTH, 0);
	
	pgl_state.gx_queue.maxEntries = 32;
	pgl_state.gx_queue.entries = (gxCmdEntry_s*)malloc(pgl_state.gx_queue.maxEntries * sizeof(gxCmdEntry_s));

	pgl_queue_init();

	pgl_state.scratch_texture = linearAlloc(SCRATCH_TEXTURE_SIZE);
	pgl_state.vertex_cache    = linearAlloc(VERTEX_BUFFER_SIZE);

	//Create render targets
	//Bottom Screen
	pgl_state.render_target[0].color_format = fb_to_rt_fmt[gfxGetScreenFormat(GFX_BOTTOM)];
	pgl_state.render_target[0].color_buffer = vramAlloc(320 * 240 * render_target_bpp[pgl_state.render_target[0].color_format]);
	pgl_state.render_target[0].depth_buffer = vramAlloc(320 * 240 * 3); // 16-bit depth
	pgl_state.render_target[0].width = 320; pgl_state.render_target[0].height = 240;
	pgl_state.render_target[0].depth_format = GPU_RB_DEPTH24;

 	//Top Screen
 	int width = gfxIsWide() ? 800 : 400;
 	pgl_state.render_target[1].color_format = fb_to_rt_fmt[gfxGetScreenFormat(GFX_TOP)];
	pgl_state.render_target[1].color_buffer = vramAlloc(width * 240 * render_target_bpp[pgl_state.render_target[1].color_format]);
	pgl_state.render_target[1].depth_buffer = vramAlloc(width * 240 * 2); // 16-bit depth
	pgl_state.render_target[1].width = width; pgl_state.render_target[1].height = 240;
	pgl_state.render_target[1].depth_format = GPU_RB_DEPTH16;

	if(pgl_state.render_target[1].color_format > GPU_RB_RGB8)
		pgl_state.downsample_textures = true;

	DVLB_s*	default_shader_dvlb = DVLB_ParseFile((u32*)vshader_shbin, vshader_shbin_size);

	shaderProgramInit(&pgl_state.default_shader_program);
	shaderProgramSetVsh(&pgl_state.default_shader_program, &default_shader_dvlb->DVLE[0]);
	shaderProgramUse(&pgl_state.default_shader_program);

	pica_attribbuffers_location((void*)__ctru_linear_heap);

	pgl_state.render_target_active = &pgl_state.render_target[1];
}

void pgl_state_default()
{
	for(int i = 0; i < 4; i++)
		pgl_texenv_reset(&pgl_state.texenv[i]);

	pgl_state.texenv[PGL_TEXENV_UNTEXTURED].src_rgb   = GPU_TEVSOURCES(GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	pgl_state.texenv[PGL_TEXENV_UNTEXTURED].src_alpha = pgl_state.texenv[PGL_TEXENV_UNTEXTURED].src_rgb;

	pgl_state.depthmap.near 	= 1.0f;
	pgl_state.depthmap.far 	= 0.0f;
	pgl_state.depthmap.offset = 0.0f;

	glViewport(0, 0, 400, 240);
	glScissor(0, 0, 400, 240);

	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glAlphaFunc(GL_ALWAYS, 0.0);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_STENCIL_TEST);

	glEnableClientState(GL_VERTEX_ARRAY);

	glClearDepth(1.0);
	glMatrixMode(GL_MODELVIEW);

	pgl_state.depth_test.write_mask = GPU_WRITE_ALL;

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ZERO);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	_picaEarlyDepthTest(false);

	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV2_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV3_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV4_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV5_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);

	pgl_state.changes = 0xffffff;
}

static void pica_texture_set(GPU_TEXUNIT unit, pgl_texture_t* texture)
{
	uint32_t addr = osConvertVirtToPhys(texture->data);

	switch (unit)
	{
	case GPU_TEXUNIT0:
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_TYPE, texture->format);
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_ADDR1, addr >> 3);
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_DIM, texture->dim);
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_PARAM, texture->param);
		GPUCMD_AddWrite(GPUREG_TEXUNIT0_LOD, texture->lod_param);
		break;

	case GPU_TEXUNIT1:
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_TYPE, texture->format);
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_ADDR, addr >> 3);
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_DIM, texture->dim);
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_PARAM, texture->param);
		GPUCMD_AddWrite(GPUREG_TEXUNIT1_LOD, texture->lod_param);
		break;

	default:
		break;
	}
}

void pica_rendertarget_set(pgl_rendertarget_t *target)
{
	static const uint8_t color_format_sizes[] = {2,1,0,0,0};

	u32 param[4] = { 0, 0, 0, 0 };

	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
	GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR, 1);
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);

	param[0] = osConvertVirtToPhys(target->depth_buffer) >> 3;
	param[1] = osConvertVirtToPhys(target->color_buffer) >> 3;
	param[2] = 0x01000000 | (((u32)(target->width-1) & 0xFFF) << 12) | (target->height & 0xFFF);
	GPUCMD_AddIncrementalWrites(GPUREG_DEPTHBUFFER_LOC, param, 3);

	GPUCMD_AddWrite(GPUREG_RENDERBUF_DIM,       param[2]);
	GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_FORMAT,  target->depth_format);
	GPUCMD_AddWrite(GPUREG_COLORBUFFER_FORMAT,  color_format_sizes[target->color_format] | ((u32)target->color_format << 16));
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_BLOCK32, 0);

	// Enable or disable color/depth buffers
	param[0] = param[1] = target->color_buffer ? 0xF : 0;
	param[2] = param[3] = target->depth_buffer ? 0x2 : 0;
	GPUCMD_AddIncrementalWrites(GPUREG_COLORBUFFER_READ, param, 4);
}

void pgl_state_flush()
{
	if(!pgl_state.changes)
		return;
	
	if(pgl_state.changes & pglDirtyFlag_RenderTarget)
	{
		pica_rendertarget_set(pgl_state.render_target_active);
	}

	if(pgl_state.changes & pglDirtyFlag_Viewport)
	{
		pica_viewport(pgl_state.viewport.y, pgl_state.viewport.x, pgl_state.viewport.height, pgl_state.viewport.width);
	}

	if(pgl_state.changes & pglDirtyFlag_Scissor)
	{
		pica_scissor_test(pgl_state.scissor.enabled, pgl_state.scissor.y, pgl_state.scissor.x, pgl_state.scissor.y + pgl_state.scissor.height, pgl_state.scissor.x + pgl_state.scissor.width);
	}

	if(pgl_state.changes & pglDirtyFlag_AlphaTest)
	{
		GPUCMD_AddWrite(GPUREG_FRAGOP_ALPHA_TEST, pgl_state.alpha_test.value);
	}

	if(pgl_state.changes & pglDirtyFlag_Blend)
	{
		if(pgl_state.blend.enabled)
		{
			GPUCMD_AddWrite(GPUREG_COLOR_OPERATION, 0xE40100);
			GPUCMD_AddWrite(GPUREG_BLEND_FUNC,  pgl_state.blend.value);
			GPUCMD_AddWrite(GPUREG_BLEND_COLOR, pgl_state.blend.color);
		}
		else
		{
			pica_logic_op(GPU_LOGICOP_COPY);
		}
	}

	if(pgl_state.changes & pglDirtyFlag_DepthMap)
	{
		pica_depthmap(pgl_state.depthmap.near, pgl_state.depthmap.far, pgl_state.depthmap.offset_enabled ? pgl_state.depthmap.offset : 0);
	}

	if(pgl_state.changes & pglDirtyFlag_DepthTest)
	{
		GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, pgl_state.depth_test.value);
	}

	if(pgl_state.changes & pglDirtyFlag_Cull)
	{
		pica_facecull_mode(pgl_state.face_cull.enabled ? pgl_state.face_cull.mode : GPU_CULL_NONE);
	}

	if(pgl_state.changes & pglDirtyFlag_TexEnv)
	{

		if(pgl_state.texunit_enabled[0])
			GPUCMD_AddIncrementalWrites(GPUREG_TEXENV0_SOURCE, (uint32_t*)&pgl_state.texenv[0], 5);
		else
			GPUCMD_AddIncrementalWrites(GPUREG_TEXENV0_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_UNTEXTURED], 5);

		if(pgl_state.texunit_enabled[1])
			GPUCMD_AddIncrementalWrites(GPUREG_TEXENV1_SOURCE, (uint32_t*)&pgl_state.texenv[1], 5);
		else
			GPUCMD_AddIncrementalWrites(GPUREG_TEXENV1_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);
	}

	if(pgl_state.changes & pglDirtyFlag_Texture)
	{

		uint16_t texunit_enable_mask = 0;

		if(pgl_state.texunit_enabled[0] && pgl_state.texture_bound[0]->data)
		{
			texunit_enable_mask |= 0x01;
			pica_texture_set(GPU_TEXUNIT0, pgl_state.texture_bound[0]);
		}

		if(pgl_state.texunit_enabled[1] && pgl_state.texture_bound[1]->data)
		{
			texunit_enable_mask |= 0x02;
			pica_texture_set(GPU_TEXUNIT1, pgl_state.texture_bound[1]);
		}

		GPUCMD_AddWrite(GPUREG_TEXUNIT_CONFIG, 0x00011000 | texunit_enable_mask);
	}

	if(pgl_state.changes & pglDirtyFlag_Mat_ModelView)
		pica_uniform_float(4, (float*)&pgl_state.matrix_stack[0][ pgl_state.matrix_stack_index[0] ], 4);
	if(pgl_state.changes & pglDirtyFlag_Mat_Projection)
		pica_uniform_float(0, (float*)&pgl_state.matrix_stack[1][ pgl_state.matrix_stack_index[1] ], 4);

	pgl_state.changes = 0;
}

static void pgl_enable_disable(GLenum cap, GLboolean state)
{
	switch (cap)
	{
		case GL_DEPTH_TEST:
			if(pgl_state.depth_test.enabled == state) return;
			pgl_state.depth_test.enabled = state;
			pgl_state.changes |= pglDirtyFlag_DepthTest;
			break;
		case GL_POLYGON_OFFSET_FILL:
			if(pgl_state.depthmap.offset_enabled == state) return;
			pgl_state.depthmap.offset_enabled = state;
			pgl_state.changes |= pglDirtyFlag_DepthTest;
			break;
		case GL_BLEND:
			if(pgl_state.blend.enabled == state) return;
			pgl_state.blend.enabled = state;
			pgl_state.changes |= pglDirtyFlag_Blend;
			break;
		case GL_SCISSOR_TEST:
			if(pgl_state.scissor.enabled == state) return;
			pgl_state.scissor.enabled = state ? 0x3 : 0x0;
			pgl_state.changes |= pglDirtyFlag_Scissor;
			break;
		case GL_CULL_FACE:
			if(pgl_state.face_cull.enabled == state) return;
			pgl_state.face_cull.enabled = state;
			pgl_state.changes |= pglDirtyFlag_Cull;
			break;
		case GL_TEXTURE_2D:
			if(pgl_state.texunit_enabled[pgl_state.texunit_active] == state) return;
			pgl_state.texunit_enabled[pgl_state.texunit_active] = state;
			pgl_state.changes |= pglDirtyFlag_TexEnv;
			break;
		case GL_ALPHA_TEST:
			if(pgl_state.alpha_test.enabled == state) return;
			pgl_state.alpha_test.enabled = state;
			pgl_state.changes |= pglDirtyFlag_AlphaTest;
			break;
		default:
			break;
	}
}

void glDisable(GLenum cap)
{
	pgl_enable_disable(cap, false);
}

void glEnable(GLenum cap)
{
	pgl_enable_disable(cap, true);
}
