#ifndef __PICA_H__
#define __PICA_H__

static inline void pica_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	u32 param[4];

	param[0] = f32tof24(width / 2.0f);
	param[1] = f32tof31(2.0f / width) << 1;
	param[2] = f32tof24(height / 2.0f);
	param[3] = f32tof31(2.0f / height) << 1;

	GPUCMD_AddIncrementalWrites(GPUREG_VIEWPORT_WIDTH, param, 4);
	GPUCMD_AddWrite(GPUREG_VIEWPORT_XY, (y << 16) | (x & 0xFFFF));
}

static inline void pica_scissor_test(GPU_SCISSORMODE mode, u32 left, u32 top, u32 right, u32 bottom)
{
	u32 param[3];

	param[0] = mode;
	param[1] = (top << 16) | (left & 0xFFFF);
	param[2] = ((bottom-1) << 16) | ((right-1) & 0xFFFF);

	GPUCMD_AddIncrementalWrites(GPUREG_SCISSORTEST_MODE, param, 3);
}

static inline void pica_facecull_mode(GPU_CULLMODE mode)
{
	GPUCMD_AddWrite(GPUREG_FACECULLING_CONFIG, mode & 0x3);
}

static inline void pica_attribbuffers_location(const void *location)
{
	uint32_t phys_location = osConvertVirtToPhys(location);
	GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_LOC, phys_location >> 3);
}

static inline void pica_attribbuffers_format(uint64_t format, uint16_t mask, uint64_t permutaion, uint8_t count)
{
	uint32_t param[2];

	param[0] = format & 0xFFFFFFFF;
	param[1] = ((count - 1) << 28) | ((mask & 0xFFF) << 16) | ((format >> 32) & 0xFFFF);

	GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFERS_FORMAT_LOW, param, 2);
	GPUCMD_AddIncrementalWrites(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, (uint32_t*)&permutaion, 2);

	GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 0xA0000000|(count - 1));
	GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, (count - 1));
}


static inline void pica_attribbuffer_config(uint8_t id, uint32_t offset, uint64_t config)
{
	uint32_t param[3];

	param[0] = offset;
	param[1] = (uint32_t)(config);
	param[2] = (uint32_t)(config >> 32);

	GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFER0_OFFSET  + (id * 3), param, 3);
}

static inline void pica_draw_arrays(GPU_Primitive_t primitive, uint32_t first, uint32_t count){

	GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG,  2, primitive);
	GPUCMD_AddMaskedWrite(GPUREG_RESTART_PRIMITIVE, 2, 1);
	GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000);
	GPUCMD_AddWrite(GPUREG_NUMVERTICES, count);
	GPUCMD_AddWrite(GPUREG_VERTEX_OFFSET, first);

	GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 1, 1);
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 0);
	GPUCMD_AddWrite(GPUREG_DRAWARRAYS, 1);
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 1);
	GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 1, 0);
	GPUCMD_AddWrite(GPUREG_VTX_FUNC, 1);

}

static inline void pica_draw_elements(GPU_Primitive_t primitive, uint32_t indexArray, uint32_t count)
{

	GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG,  2, primitive != GPU_TRIANGLES ? primitive : GPU_GEOMETRY_PRIM);
	GPUCMD_AddMaskedWrite(GPUREG_RESTART_PRIMITIVE, 2, 1);
	GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000 | indexArray);
	GPUCMD_AddWrite(GPUREG_NUMVERTICES, count);
	GPUCMD_AddWrite(GPUREG_VERTEX_OFFSET, 0);

	if (primitive == GPU_TRIANGLES)
	{
		GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG,  2, 0x100);
		GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 2, 0x100);
	}

	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 0);
	GPUCMD_AddWrite(GPUREG_DRAWELEMENTS, 1);
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 1);

	if (primitive == GPU_TRIANGLES)
	{
		GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG,  2, 0);
		GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 2, 0);
	}

	GPUCMD_AddWrite(GPUREG_VTX_FUNC, 1);
}

static inline void pica_depthmap(float near, float far, float polygon_offset){
	float scale  = near - far;
	float offset = near + polygon_offset;

	GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, 1);
	GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE,  f32tof24(scale));
	GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, f32tof24(offset));
}

static inline void pica_early_depth_test(bool enabled)
{
	GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST1, enabled);
	GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, enabled);
}

static inline void pica_uniform_float(uint32_t startreg, float *data, uint32_t numreg)
{
	GPUCMD_AddWrite (GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | startreg);
	GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)data, numreg);
}

static inline void pica_uniform_matf(uint32_t startreg, float *data)
{
	GPUCMD_AddWrite (GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | startreg);
	GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)data, 16);
}

static inline void pica_uniforms_bool(uint16_t data)
{

	GPUCMD_AddWrite(GPUREG_VSH_BOOLUNIFORM, 0x7FFF0000 | data);
}

static inline void pica_depth_color_mask(bool enable, GPU_TESTFUNC function, GPU_WRITEMASK writemask)
{
	GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, (enable & 1) | ((function & 7) << 4) | (writemask << 8));
}


static inline void pica_immediate_begin(GPU_Primitive_t primitive)
{
	GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 2, primitive);
	GPUCMD_AddWrite(GPUREG_RESTART_PRIMITIVE, 1);
	GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000);
	GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 1, 1);
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 0);
	GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 0xF);
}

static inline void pica_immediate_end(void)
{
	GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 1);
	GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 1, 0);
	GPUCMD_AddWrite(GPUREG_VTX_FUNC, 1);
}

static inline void pica_fixed_attribute(float x, float y, float z, float w)
{
	union {
		u32 packed[4];
		struct { u8 x[3], y[3], z[3], w[3]; };
	} param;

	// Convert the values to float24
	*((uint32_t*)param.x) = f32tof24(x);
	*((uint32_t*)param.y) = f32tof24(y);
	*((uint32_t*)param.z) = f32tof24(z);
	*((uint32_t*)param.w) = f32tof24(w);

	// Reverse the packed words
	u32 p = param.packed[0];
	param.packed[0] = param.packed[2];
	param.packed[2] = p;

	// Send the attribute
	GPUCMD_AddIncrementalWrites(GPUREG_FIXEDATTRIB_DATA0, param.packed, 3);
}

static inline void pica_logic_op(GPU_LOGICOP op)
{
	GPUCMD_AddWrite(GPUREG_LOGIC_OP, op);
	GPUCMD_AddWrite(GPUREG_COLOR_OPERATION, 0xE40000);
}

#endif //__PICA_H__