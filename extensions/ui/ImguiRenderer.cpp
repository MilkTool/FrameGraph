// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#include "ImguiRenderer.h"
#include "imgui_internal.h"

namespace FG
{

/*
=================================================
	constructor
=================================================
*/
	ImguiRenderer::ImguiRenderer ()
	{
	}
	
/*
=================================================
	Initialize
=================================================
*/
	bool ImguiRenderer::Initialize (const FGThreadPtr &fg, ImGuiContext *ctx)
	{
		CHECK_ERR( ctx );

		_context = ctx;

		CHECK_ERR( _CreatePipeline( fg ));
		CHECK_ERR( _CreateSampler( fg ));
		
		// initialize font atlas
		{
			uint8_t*	pixels;
			int			width, height;
			_context->IO.Fonts->GetTexDataAsRGBA32( OUT &pixels, OUT &width, OUT &height );
		}
		return true;
	}
	
/*
=================================================
	Deinitialize
=================================================
*/
	void ImguiRenderer::Deinitialize (const FGThreadPtr &fg)
	{
		fg->DestroyResource( INOUT _fontTexture );
		fg->DestroyResource( INOUT _fontSampler );
		fg->DestroyResource( INOUT _pipeline );
		fg->DestroyResource( INOUT _vertexBuffer );
		fg->DestroyResource( INOUT _indexBuffer );
		fg->DestroyResource( INOUT _uniformBuffer );

		_context = null;
	}
	
/*
=================================================
	Draw
=================================================
*/
	Task  ImguiRenderer::Draw (const FGThreadPtr &fg, LogicalPassID passId)
	{
		CHECK_ERR( fg and _context and _context->DrawData.Valid );

		ImDrawData&		draw_data = _context->DrawData;
		ASSERT( draw_data.TotalVtxCount > 0 );

		SubmitRenderPass	submit {passId};

		submit.DependsOn( _CreateFontTexture( fg ));
		submit.DependsOn( _RecreateBuffers( fg ));
		submit.DependsOn( _UpdateUniformBuffer( fg ));

		RenderState		rs;
		rs.AddColorBuffer( RenderTargetID("out_Color0"), EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha, EBlendOp::Add );
		rs.SetDepthTestEnabled( false );
		rs.SetCullMode( ECullMode::None );
		rs.SetTopology( EPrimitive::TriangleList );

		VertexInputState	vert_input;
		vert_input.Bind( VertexBufferID(), SizeOf<ImDrawVert> );
		vert_input.Add( VertexID("aPos"),   EVertexType::Float2,  OffsetOf( &ImDrawVert::pos ) );
		vert_input.Add( VertexID("aUV"),    EVertexType::Float2,  OffsetOf( &ImDrawVert::uv  ) );
		vert_input.Add( VertexID("aColor"), EVertexType::UByte4_Norm, OffsetOf( &ImDrawVert::col ) );
		  

		uint	idx_offset	= 0;
		uint	vtx_offset	= 0;

		_resources.BindBuffer( UniformID("uPushConstant"), _uniformBuffer );
		
		for (int i = 0; i < draw_data.CmdListsCount; ++i)
		{
			const ImDrawList &	cmd_list	= *draw_data.CmdLists[i];

			for (int j = 0; j < cmd_list.CmdBuffer.Size; ++j)
			{
				const ImDrawCmd &	cmd = cmd_list.CmdBuffer[j];

				if ( cmd.UserCallback )
				{
					cmd.UserCallback( &cmd_list, &cmd );
				}
				else
				{
					RectI	scissor;
					scissor.left	= int(cmd.ClipRect.x - draw_data.DisplayPos.x + 0.5f) > 0 ? int(cmd.ClipRect.x - draw_data.DisplayPos.x + 0.5f) : 0;
					scissor.top		= int(cmd.ClipRect.y - draw_data.DisplayPos.y + 0.5f) > 0 ? int(cmd.ClipRect.y - draw_data.DisplayPos.y + 0.5f) : 0;
					scissor.right	= int(cmd.ClipRect.z + 0.5f);
					scissor.bottom	= int(cmd.ClipRect.w + 0.5f);

					_resources.BindTexture( UniformID("sTexture"), _fontTexture, _fontSampler );

					fg->AddDrawTask( passId, DrawIndexedTask{}
								.SetRenderState( rs ).SetPipeline( _pipeline ).AddResources( 0, &_resources )
								.AddBuffer( VertexBufferID(), _vertexBuffer ).SetVertexInput( vert_input )
								.SetIndexBuffer( _indexBuffer, 0_b, EIndex::UShort )
								.SetDrawCmd({ cmd.ElemCount, 1, idx_offset, int(vtx_offset), 0 })
								.AddScissor( scissor ).SetDynamicStates( EPipelineDynamicState::Viewport | EPipelineDynamicState::Scissor )
								//.AddPushConstant( PushConstantID("pc"), pc_data )
							);
				}
				idx_offset += cmd.ElemCount;
			}

			vtx_offset += cmd_list.VtxBuffer.Size;
		}

		return fg->AddTask( submit );
	}

/*
=================================================
	_CreatePipeline
=================================================
*/
	bool ImguiRenderer::_CreatePipeline (const FGThreadPtr &fg)
	{
		using namespace std::string_literals;

		GraphicsPipelineDesc	desc;
		
		desc.AddShader( EShader::Vertex, EShaderLangFormat::VKSL_100, "main", R"#(
			#version 450 core
			layout(location = 0) in vec2 aPos;
			layout(location = 1) in vec2 aUV;
			layout(location = 2) in vec4 aColor;

			//layout(push_constant) uniform uPushConstant {
			layout(set=0, binding=1, std140) uniform uPushConstant {
				vec2 uScale;
				vec2 uTranslate;
			} pc;

			out gl_PerVertex{
				vec4 gl_Position;
			};

			layout(location = 0) out struct{
				vec4 Color;
				vec2 UV;
			} Out;

			void main()
			{
				Out.Color = aColor;
				Out.UV = aUV;
				gl_Position = vec4(aPos*pc.uScale+pc.uTranslate, 0, 1);
			})#"s );
		
		desc.AddShader( EShader::Fragment, EShaderLangFormat::VKSL_100, "main", R"#(
			#version 450 core
			layout(location = 0) out vec4 out_Color0;

			layout(set=0, binding=0) uniform sampler2D sTexture;

			layout(location = 0) in struct{
				vec4 Color;
				vec2 UV;
			} In;

			void main()
			{
				out_Color0 = In.Color * texture(sTexture, In.UV.st);
			})#"s );

		_pipeline = fg->CreatePipeline( std::move(desc) );
		CHECK_ERR( _pipeline );

		RawDescriptorSetLayoutID	ds_layout;
		uint						binding = 0;
		CHECK_ERR( fg->GetDescriptorSet( _pipeline, DescriptorSetID("0"), OUT ds_layout, OUT binding ));
		CHECK_ERR( binding == 0 );

		CHECK_ERR( fg->InitPipelineResources( ds_layout, OUT _resources ));
		return true;
	}
	
/*
=================================================
	_CreateSampler
=================================================
*/
	bool  ImguiRenderer::_CreateSampler (const FGThreadPtr &fg)
	{
		SamplerDesc		desc;
		desc.magFilter		= EFilter::Linear;
		desc.minFilter		= EFilter::Linear;
		desc.mipmapMode		= EMipmapFilter::Linear;
		desc.addressMode	= { EAddressMode::Repeat };
		desc.minLod			= -1000.0f;
		desc.maxLod			= 1000.0f;

		_fontSampler = fg->CreateSampler( desc );
		CHECK_ERR( _fontSampler );
		return true;
	}

/*
=================================================
	_CreateFontTexture
=================================================
*/
	Task  ImguiRenderer::_CreateFontTexture (const FGThreadPtr &fg)
	{
		if ( _fontTexture )
			return null;

		uint8_t*	pixels;
		int			width, height;

		_context->IO.Fonts->GetTexDataAsRGBA32( OUT &pixels, OUT &width, OUT &height );

		size_t		upload_size = width * height * 4 * sizeof(char);

		_fontTexture = fg->CreateImage( MemoryDesc{}, ImageDesc{ EImage::Tex2D, uint3{uint(width), uint(height), 1},
																 EPixelFormat::RGBA8_UNorm, EImageUsage::Sampled | EImageUsage::TransferDst });
		CHECK_ERR( _fontTexture );

		return fg->AddTask( UpdateImage{}.SetImage( _fontTexture ).SetData( pixels, upload_size, uint2{int2{ width, height }} ));
	}
	
/*
=================================================
	_UpdateUniformBuffer
=================================================
*/
	Task  ImguiRenderer::_UpdateUniformBuffer (const FGThreadPtr &fg)
	{
		ImDrawData &	draw_data = _context->DrawData;

		if ( not _uniformBuffer )
		{
			_uniformBuffer = fg->CreateBuffer( MemoryDesc{}, BufferDesc{ 16_b, EBufferUsage::Uniform | EBufferUsage::TransferDst });
			CHECK_ERR( _uniformBuffer );
		}
		
		float4		pc_data;
		// scale:
		pc_data[0] = 2.0f / (draw_data.DisplaySize.x * _context->IO.DisplayFramebufferScale.x);
		pc_data[1] = 2.0f / (draw_data.DisplaySize.y * _context->IO.DisplayFramebufferScale.y);
		// transform:
		pc_data[2] = -1.0f - draw_data.DisplayPos.x * pc_data[0];
		pc_data[3] = -1.0f - draw_data.DisplayPos.y * pc_data[1];

		return fg->AddTask( UpdateBuffer{}.SetBuffer( _uniformBuffer ).SetData( &pc_data, 1 ));
	}

/*
=================================================
	_RecreateBuffers
=================================================
*/
	Task  ImguiRenderer::_RecreateBuffers (const FGThreadPtr &fg)
	{
		ImDrawData &	draw_data	= _context->DrawData;
		size_t			vertex_size	= draw_data.TotalVtxCount * sizeof(ImDrawVert);
		size_t			index_size	= draw_data.TotalIdxCount * sizeof(ImDrawIdx);

		if ( not _vertexBuffer or vertex_size > _vertexBufSize )
		{
			fg->DestroyResource( INOUT _vertexBuffer );

			_vertexBufSize	= vertex_size;
			_vertexBuffer	= fg->CreateBuffer( MemoryDesc{}, BufferDesc{ BytesU(vertex_size), EBufferUsage::TransferDst | EBufferUsage::Vertex });
		}

		if ( not _indexBuffer or index_size > _indexBufSize )
		{
			fg->DestroyResource( INOUT _indexBuffer );

			_indexBufSize	= index_size;
			_indexBuffer	= fg->CreateBuffer( MemoryDesc{}, BufferDesc{ BytesU(index_size), EBufferUsage::TransferDst | EBufferUsage::Index });
		}

		BytesU	vb_offset;
		BytesU	ib_offset;
		
		Task	last_task;

		for (int i = 0; i < draw_data.CmdListsCount; ++i)
		{
			const ImDrawList &	cmd_list = *draw_data.CmdLists[i];
			
			last_task = fg->AddTask( UpdateBuffer{}.SetBuffer( _vertexBuffer, vb_offset ).SetData( cmd_list.VtxBuffer.Data, cmd_list.VtxBuffer.Size ).DependsOn( last_task ));
			last_task = fg->AddTask( UpdateBuffer{}.SetBuffer( _indexBuffer, ib_offset ).SetData( cmd_list.IdxBuffer.Data, cmd_list.IdxBuffer.Size ).DependsOn( last_task ));

			vb_offset += cmd_list.VtxBuffer.Size * SizeOf<ImDrawVert>;
			ib_offset += cmd_list.IdxBuffer.Size * SizeOf<ImDrawIdx>;
		}

		return last_task;
	}


}	// FG