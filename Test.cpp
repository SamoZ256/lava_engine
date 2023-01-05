
		//Main render pass
		mainRenderPass.framebuffer.bind();

		mainGraphicsPipeline.bind();
		//texture.image.bind(0);
		//texture.sampler.bind(0);

		deferredRenderPass.normalRoughnessAttachment.bind(0);
		deferredRenderPass.normalRoughnessAttachmentSampler.bind(0);

		deferredRenderPass.albedoMetallicAttachment.bind(1);
		deferredRenderPass.albedoMetallicAttachmentSampler.bind(1);

		deferredRenderPass.depthAttachment.bind(2);
		deferredRenderPass.depthAttachmentSampler.bind(2);

		shadowRenderPass.depthAttachment.bind(3);
		shadowRenderPass.depthAttachmentSampler.bind(3);

		UBOMainVP uboMainVP{glm::inverse(camera.projection * camera.view), camera.position};
		mainGraphicsPipeline.uploadPushConstants(&uboMainVP, 1, sizeof(UBOMainVP), LV_SHADER_STAGE_FRAGMENT_BIT);

		directLight.uploadUniforms();
		directLight.lightUniformBuffer.bindToFragmentShader(0);

		mainGraphicsPipeline.uploadPushConstants(shadowVPs.data(), 2, shadowVPs.size() * sizeof(glm::mat4), LV_SHADER_STAGE_FRAGMENT_BIT);

		swapChain.renderFullscreenTriangle();
		//}

		//vertexBuffer.bindToVertexShader(0);
		//indexBuffer.renderIndexed(MTL::IndexType::IndexTypeUInt32, sizeof(uint32_t));

		mainRenderPass.framebuffer.unbind();

		mainRenderPass.framebuffer.render();
