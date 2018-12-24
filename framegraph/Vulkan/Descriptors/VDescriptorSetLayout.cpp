// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#include "VDescriptorSetLayout.h"
#include "VEnumCast.h"
#include "VDevice.h"

namespace FG
{

/*
=================================================
	TextureEquals
=================================================
*/
	ND_ inline bool TextureEquals (const PipelineDescription::Texture &lhs, const PipelineDescription::Texture &rhs) noexcept
	{
		return	lhs.textureType		== rhs.textureType	and
				lhs.state			== rhs.state;
	}

/*
=================================================
	SamplerEquals
=================================================
*/
	ND_ inline bool SamplerEquals (const PipelineDescription::Sampler &, const PipelineDescription::Sampler &) noexcept
	{
		return true;
	}

/*
=================================================
	SubpassInputEquals
=================================================
*/
	ND_ inline bool SubpassInputEquals (const PipelineDescription::SubpassInput &lhs, const PipelineDescription::SubpassInput &rhs) noexcept
	{
		return	lhs.attachmentIndex		== rhs.attachmentIndex		and
				lhs.isMultisample		== rhs.isMultisample		and
				lhs.state				== rhs.state;
	}
	
/*
=================================================
	ImageEquals
=================================================
*/
	ND_ inline bool ImageEquals (const PipelineDescription::Image &lhs, const PipelineDescription::Image &rhs) noexcept
	{
		return	lhs.imageType	== rhs.imageType	and
				lhs.format		== rhs.format		and
				lhs.state		== rhs.state;
	}
	
/*
=================================================
	UniformBufferEquals
=================================================
*/
	ND_ inline bool UniformBufferEquals (const PipelineDescription::UniformBuffer &lhs, const PipelineDescription::UniformBuffer &rhs) noexcept
	{
		return	lhs.size				== rhs.size					and
				lhs.dynamicOffsetIndex	== rhs.dynamicOffsetIndex	and
				lhs.state				== rhs.state;
	}
	
/*
=================================================
	StorageBufferEquals
=================================================
*/
	ND_ inline bool StorageBufferEquals (const PipelineDescription::StorageBuffer &lhs, const PipelineDescription::StorageBuffer &rhs) noexcept
	{
		return	lhs.staticSize			== rhs.staticSize			and
				lhs.arrayStride			== rhs.arrayStride			and
				lhs.dynamicOffsetIndex	== rhs.dynamicOffsetIndex	and
				lhs.state				== rhs.state;
	}
	
/*
=================================================
	RayTracingSceneEquals
=================================================
*/
	ND_ inline bool RayTracingSceneEquals (const PipelineDescription::RayTracingScene &lhs, const PipelineDescription::RayTracingScene &rhs) noexcept
	{
		return	lhs.state	== rhs.state;
	}
//-----------------------------------------------------------------------------
	

	
/*
=================================================
	UniformEquals
=================================================
*/
	ND_ inline bool UniformEquals (const PipelineDescription::Uniform &lhsUniform, const PipelineDescription::Uniform &rhsUniform) noexcept
	{
		if ( lhsUniform.index.VKBinding()	!= rhsUniform.index.VKBinding()  or
			 lhsUniform.stageFlags			!= rhsUniform.stageFlags )
		{
			return false;
		}

		bool	result = false;

		Visit( lhsUniform.data,
			[&rhsUniform, &result] (const PipelineDescription::Texture &lhs)
			{
				if ( auto* rhs = std::get_if<PipelineDescription::Texture>( &rhsUniform.data ) )
				{
					result = TextureEquals( lhs, *rhs );
				}
			},
				   
			[&rhsUniform, &result] (const PipelineDescription::Sampler &lhs)
			{
				if ( auto* rhs = std::get_if<PipelineDescription::Sampler>( &rhsUniform.data ) )
				{
					result = SamplerEquals( lhs, *rhs );
				}
			},
				
			[&rhsUniform, &result] (const PipelineDescription::SubpassInput &lhs)
			{
				if ( auto* rhs = std::get_if<PipelineDescription::SubpassInput>( &rhsUniform.data ) )
				{
					result = SubpassInputEquals( lhs, *rhs );
				}
			},
				
			[&rhsUniform, &result] (const PipelineDescription::Image &lhs)
			{
				if ( auto* rhs = std::get_if<PipelineDescription::Image>( &rhsUniform.data ) )
				{
					result = ImageEquals( lhs, *rhs );
				}
			},
				
			[&rhsUniform, &result] (const PipelineDescription::UniformBuffer &lhs)
			{
				if ( auto* rhs = std::get_if<PipelineDescription::UniformBuffer>( &rhsUniform.data ) )
				{
					result = UniformBufferEquals( lhs, *rhs );
				}
			},
				
			[&rhsUniform, &result] (const PipelineDescription::StorageBuffer &lhs)
			{
				if ( auto* rhs = std::get_if<PipelineDescription::StorageBuffer>( &rhsUniform.data ) )
				{
					result = StorageBufferEquals( lhs, *rhs );
				}
			},
				
			[&rhsUniform, &result] (const PipelineDescription::RayTracingScene &lhs)
			{
				if ( auto* rhs = std::get_if<PipelineDescription::RayTracingScene>( &rhsUniform.data ) )
				{
					result = RayTracingSceneEquals( lhs, *rhs );
				}
			},

			[] (const std::monostate &) { ASSERT(false); }
		);

		return result;
	}
//-----------------------------------------------------------------------------
	
	

/*
=================================================
	destructor
=================================================
*/
	VDescriptorSetLayout::~VDescriptorSetLayout ()
	{
		CHECK( _layout == VK_NULL_HANDLE );
	}
	
/*
=================================================
	constructor
=================================================
*/
	VDescriptorSetLayout::VDescriptorSetLayout (const UniformMapPtr &uniforms, OUT DescriptorBinding_t &binding)
	{
		SCOPELOCK( _rcCheck );
		ASSERT( uniforms );
		ASSERT( not _uniforms );
		ASSERT( _layout == VK_NULL_HANDLE );

		_uniforms = uniforms;

		// bind uniforms
		binding.clear();
		binding.reserve( _uniforms->size() );

		for (auto& un : *_uniforms)
		{
			ASSERT( un.first.IsDefined() );

			_hash << HashOf( un.first );

			_AddUniform( un.second, INOUT binding );
		}
	}
	
/*
=================================================
	Create
=================================================
*/
	bool VDescriptorSetLayout::Create (const VDevice &dev, const DescriptorBinding_t &binding)
	{
		SCOPELOCK( _rcCheck );
		CHECK_ERR( _layout == VK_NULL_HANDLE );

		VkDescriptorSetLayoutCreateInfo	descriptor_info = {};
		descriptor_info.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_info.pBindings		= binding.data();
		descriptor_info.bindingCount	= uint(binding.size());

		VK_CHECK( dev.vkCreateDescriptorSetLayout( dev.GetVkDevice(), &descriptor_info, null, OUT &_layout ) );
		return true;
	}
	
/*
=================================================
	Destroy
=================================================
*/
	void VDescriptorSetLayout::Destroy (OUT AppendableVkResources_t readyToDelete, OUT AppendableResourceIDs_t)
	{
		SCOPELOCK( _rcCheck );

		if ( _layout ) {
			readyToDelete.emplace_back( VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, uint64_t(_layout) );
		}

		_poolSize.clear();
		_uniforms	= null;
		_layout		= VK_NULL_HANDLE;
		_hash		= Default;
	}

/*
=================================================
	_AddUniform
=================================================
*/
	void VDescriptorSetLayout::_AddUniform (const PipelineDescription::Uniform &un, INOUT DescriptorBinding_t &binding)
	{
		ASSERT( un.index.VKBinding() != UMax );

		Visit( un.data,
			[&] (const PipelineDescription::Texture &tex) {
				_AddTexture( tex, un.index.VKBinding(), un.stageFlags, INOUT binding );
			},
			[&] (const PipelineDescription::Sampler &samp) {
				_AddSampler( samp, un.index.VKBinding(), un.stageFlags, INOUT binding );
			},
			[&] (const PipelineDescription::SubpassInput &spi) {
				_AddSubpassInput( spi, un.index.VKBinding(), un.stageFlags, INOUT binding );
			},
			[&] (const PipelineDescription::Image &img)	{
				_AddImage( img, un.index.VKBinding(), un.stageFlags, INOUT binding );
			},
			[&] (const PipelineDescription::UniformBuffer &ub) {
				_AddUniformBuffer( ub, un.index.VKBinding(), un.stageFlags, INOUT binding );
			},
			[&] (const PipelineDescription::StorageBuffer &sb) {
				_AddStorageBuffer( sb, un.index.VKBinding(), un.stageFlags, INOUT binding );
			},
			[&] (const PipelineDescription::RayTracingScene &rts) {
				_AddRayTracingScene( rts, un.index.VKBinding(), un.stageFlags, INOUT binding );
			},
			[] (const std::monostate &) {
				ASSERT(false);
			}
		);
	}
	
/*
=================================================
	_IncDescriptorCount
=================================================
*/
	void VDescriptorSetLayout::_IncDescriptorCount (VkDescriptorType type)
	{
		for (auto& size : _poolSize)
		{
			if ( size.type == type )
			{
				++size.descriptorCount;
				return;
			}
		}

		_poolSize.emplace_back( type, 1u );
	}

/*
=================================================
	_AddImage
=================================================
*/
	void VDescriptorSetLayout::_AddImage (const PipelineDescription::Image &img, uint bindingIndex,
										  EShaderStages stageFlags, INOUT DescriptorBinding_t &binding)
	{
		// calculate hash
		_hash << HashOf( img.imageType )
			  << HashOf( img.format )
			  << HashOf( bindingIndex )
			  << HashOf( stageFlags )
			  << HashOf( img.state );
		
		// add binding
		VkDescriptorSetLayoutBinding	bind = {};
		bind.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		bind.stageFlags			= VEnumCast( stageFlags );
		bind.binding			= bindingIndex;
		bind.descriptorCount	= 1;

		_maxIndex = Max( _maxIndex, bind.binding );
		_IncDescriptorCount( bind.descriptorType );

		binding.push_back( std::move(bind) );
	}
	
/*
=================================================
	_AddTexture
=================================================
*/
	void VDescriptorSetLayout::_AddTexture (const PipelineDescription::Texture &tex, uint bindingIndex,
											EShaderStages stageFlags, INOUT DescriptorBinding_t &binding)
	{
		// calculate hash
		_hash << HashOf( tex.textureType )
			  << HashOf( bindingIndex )
			  << HashOf( stageFlags )
			  << HashOf( tex.state );

		// add binding
		VkDescriptorSetLayoutBinding	bind = {};
		bind.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bind.stageFlags			= VEnumCast( stageFlags );
		bind.binding			= bindingIndex;
		bind.descriptorCount	= 1;
		
		_maxIndex = Max( _maxIndex, bind.binding );
		_IncDescriptorCount( bind.descriptorType );

		binding.push_back( std::move(bind) );
	}
	
/*
=================================================
	_AddSampler
=================================================
*/
	void VDescriptorSetLayout::_AddSampler (const PipelineDescription::Sampler &, uint bindingIndex,
											EShaderStages stageFlags, INOUT DescriptorBinding_t &binding)
	{
		// calculate hash
		_hash << HashOf( bindingIndex )
			  << HashOf( stageFlags );
		
		// add binding
		VkDescriptorSetLayoutBinding	bind = {};
		bind.descriptorType		= VK_DESCRIPTOR_TYPE_SAMPLER;
		bind.stageFlags			= VEnumCast( stageFlags );
		bind.binding			= bindingIndex;
		bind.descriptorCount	= 1;
		
		_maxIndex = Max( _maxIndex, bind.binding );
		_IncDescriptorCount( bind.descriptorType );

		binding.push_back( std::move(bind) );
	}
	
/*
=================================================
	_AddSubpassInput
=================================================
*/
	void VDescriptorSetLayout::_AddSubpassInput (const PipelineDescription::SubpassInput &spi, uint bindingIndex,
												 EShaderStages stageFlags, INOUT DescriptorBinding_t &binding)
	{
		// calculate hash
		_hash << HashOf( spi.attachmentIndex )
			  << HashOf( spi.isMultisample )
			  << HashOf( bindingIndex )
			  << HashOf( stageFlags )
			  << HashOf( spi.state );
		
		// add binding
		VkDescriptorSetLayoutBinding	bind = {};
		bind.descriptorType		= VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		bind.stageFlags			= VEnumCast( stageFlags );
		bind.binding			= bindingIndex;
		bind.descriptorCount	= 1;
		
		_maxIndex = Max( _maxIndex, bind.binding );
		_IncDescriptorCount( bind.descriptorType );

		binding.push_back( std::move(bind) );
	}
	
/*
=================================================
	_AddUniformBuffer
=================================================
*/
	void VDescriptorSetLayout::_AddUniformBuffer (const PipelineDescription::UniformBuffer &ub, uint bindingIndex,
												  EShaderStages stageFlags, INOUT DescriptorBinding_t &binding)
	{
		// calculate hash
		_hash << HashOf( ub.size )
			  << HashOf( bindingIndex )
			  << HashOf( stageFlags )
			  << HashOf( ub.state );

		// add binding
		VkDescriptorSetLayoutBinding	bind = {};
		bind.descriptorType		= EnumEq( ub.state, EResourceState::_BufferDynamicOffset ) ?
									VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bind.stageFlags			= VEnumCast( stageFlags );
		bind.binding			= bindingIndex;
		bind.descriptorCount	= 1;
		
		_maxIndex = Max( _maxIndex, bind.binding );
		_IncDescriptorCount( bind.descriptorType );

		binding.push_back( std::move(bind) );
	}
	
/*
=================================================
	_AddStorageBuffer
=================================================
*/
	void VDescriptorSetLayout::_AddStorageBuffer (const PipelineDescription::StorageBuffer &sb, uint bindingIndex,
												  EShaderStages stageFlags, INOUT DescriptorBinding_t &binding)
	{
		// calculate hash
		_hash << HashOf( sb.staticSize )
			  << HashOf( sb.arrayStride )
			  << HashOf( bindingIndex )
			  << HashOf( stageFlags )
			  << HashOf( sb.state );
		
		// add binding
		VkDescriptorSetLayoutBinding	bind = {};
		bind.descriptorType		= EnumEq( sb.state, EResourceState::_BufferDynamicOffset ) ?
									VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bind.stageFlags			= VEnumCast( stageFlags );
		bind.binding			= bindingIndex;
		bind.descriptorCount	= 1;
		
		_maxIndex = Max( _maxIndex, bind.binding );
		_IncDescriptorCount( bind.descriptorType );

		binding.push_back( std::move(bind) );
	}
	
/*
=================================================
	_AddRayTracingScene
=================================================
*/
	void VDescriptorSetLayout::_AddRayTracingScene (const PipelineDescription::RayTracingScene &rts, uint bindingIndex,
													EShaderStages stageFlags, INOUT DescriptorBinding_t &binding)
	{
		// calculate hash
		_hash << HashOf( rts.state );

		// add binding
		VkDescriptorSetLayoutBinding	bind = {};
		bind.descriptorType		= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
		bind.stageFlags			= VEnumCast( stageFlags );
		bind.binding			= bindingIndex;
		bind.descriptorCount	= 1;

		_maxIndex = Max( _maxIndex, bind.binding );
		_IncDescriptorCount( bind.descriptorType );

		binding.push_back( std::move(bind) );
	}

/*
=================================================
	operator ==
=================================================
*/
	bool VDescriptorSetLayout::operator == (const VDescriptorSetLayout &rhs) const
	{
		SHAREDLOCK( _rcCheck );
		SHAREDLOCK( rhs._rcCheck );

		if ( _hash != rhs._hash					or
			 not (_uniforms and rhs._uniforms)	or
			_uniforms->size() != rhs._uniforms->size() )
		{
			return false;
		}

		for (auto& un : *_uniforms)
		{
			auto	iter = rhs._uniforms->find( un.first );

			if ( iter == rhs._uniforms->end() )
				return false;
			
			if ( not UniformEquals( un.second, iter->second ) )
				return false;
		}
		return true;
	}
	

}	// FG