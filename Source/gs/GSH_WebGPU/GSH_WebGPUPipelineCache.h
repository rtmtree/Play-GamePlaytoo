#pragma once

#include "GSH_WebGPUContext.h"
#include <unordered_map>

namespace GSH_WebGPU
{
	struct PIPELINE
	{
		wgpu::BindGroupLayout bindGroupLayout;
		wgpu::PipelineLayout pipelineLayout;
		wgpu::RenderPipeline pipeline;
	};

	template <typename KeyType>
	class CPipelineCache
	{
	public:
		CPipelineCache(wgpu::Device& device)
		    : m_device(device)
		{
		}

		~CPipelineCache()
		{
			// WebGPU objects are ref-counted and will be released automatically
			// when the map is cleared or the cache is destroyed.
		}

		const PIPELINE* TryGetPipeline(const KeyType& key) const
		{
			auto pipelineIterator = m_pipelines.find(key);
			return (pipelineIterator == std::end(m_pipelines)) ? nullptr : &pipelineIterator->second;
		}

		const PIPELINE* RegisterPipeline(const KeyType& key, const PIPELINE& pipeline)
		{
			m_pipelines.insert(std::make_pair(key, pipeline));
			return TryGetPipeline(key);
		}
		
		void Clear()
		{
			m_pipelines.clear();
		}

	private:
		typedef std::unordered_map<KeyType, PIPELINE> PipelineMap;

		wgpu::Device m_device;
		PipelineMap m_pipelines;
	};
}
