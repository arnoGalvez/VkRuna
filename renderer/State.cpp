#include "State.h"

namespace vkRuna
{
uint64_t StateHelper( uint64_t state, pipelineState_t option, pipelineState_t mask )
{
	state &= ~mask;
	state |= option;
	return state;
}

uint64_t StateSetSrcBlend( uint64_t state, pipelineState_t srcBlend )
{
	return StateHelper( state, srcBlend, SRCBLEND_FACTOR_MASK );
}

uint64_t StateSetDstBlend( uint64_t state, pipelineState_t dstBlend )
{
	return StateHelper( state, dstBlend, DSTBLEND_FACTOR_MASK );
}

uint64_t StateSetBlendOp( uint64_t state, pipelineState_t blendOp )
{
	return StateHelper( state, blendOp, BLEND_OP_MASK );
}

uint64_t StateSetCullMode( uint64_t state, pipelineState_t cullMode )
{
	return StateHelper( state, cullMode, CULL_MODE_MASK );
}

uint64_t StateSetPolygonMode( uint64_t state, pipelineState_t polygonMode )
{
	return StateHelper( state, polygonMode, POLYGON_MODE_MASK );
}

uint64_t StateSetPrimitiveTopology( uint64_t state, pipelineState_t primitiveTopology )
{
	return StateHelper( state, primitiveTopology, PRIMITIVE_TOPOLOGY_MASK );
}

uint64_t StateSetDepthTest( uint64_t state, bool enabled )
{
	state &= ~DEPTH_TEST_ENABLE;
	state |= uint64_t( enabled ) * DEPTH_TEST_ENABLE;
	return state;
}

uint64_t StateSetDepthWrite( uint64_t state, bool enabled )
{
	state &= ~DEPTH_WRITE_ENABLE;
	state |= uint64_t( enabled ) * DEPTH_WRITE_ENABLE;
	return state;
}

uint64_t StateSetDepthOp( uint64_t state, pipelineState_t depthOp )
{
	return StateHelper( state, depthOp, DEPTH_COMPARE_OP_MASK );
}

} // namespace vkRuna
