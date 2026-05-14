//
// Created by y1 on 2026-05-14.
//

#include "Pass.h"

#include "DXDebug.h"
#include "JsonFile.h"

Pass::Pass(DXApp &dxApp, std::string_view inputFile)
    : m_app(dxApp) {
    const JsonFile json{inputFile};
    m_name = json.Get<std::string>("name");

    // Graphics pipeline configs always carry "topology"; compute configs don't
    const std::string topology = json.Get<std::string>("topology", "");
    m_type                     = topology.empty() ? PipelineType::Compute : PipelineType::Graphics;

    if (m_type == PipelineType::Graphics) {
        m_pipeline.emplace<DXGraphicsPipeline>(m_app.CreateGraphicsPipeline(inputFile));
    } else {
        m_pipeline.emplace<DXComputePipeline>(m_app.CreateComputePipeline(inputFile));
    }
}

void Pass::Execute(const DrawContext &context) {
    drez::dx::debug::ScopedEvent passScope{context.commandList, m_name};
    context.timestamps->BeginPass(context.commandList, m_name);

    TransitionBarriers(context);
    BindPipeline(context.commandList);
    BindResources(context);
    Record(context);
    FinalizeBarriers(context);

    context.timestamps->EndPass(context.commandList);
}

void Pass::BindPipeline(ID3D12GraphicsCommandList *commandList) {
    switch (m_type) {
        case PipelineType::Graphics: {
            const auto &pipeline = std::get<DXGraphicsPipeline>(m_pipeline);
            commandList->SetGraphicsRootSignature(pipeline.GetRootSignature());
            commandList->SetPipelineState(pipeline.GetPipelineState());
            commandList->IASetPrimitiveTopology(pipeline.GetPrimitiveTopology());
            break;
        }
        case PipelineType::Compute: {
            const auto &pipeline = std::get<DXComputePipeline>(m_pipeline);
            commandList->SetComputeRootSignature(pipeline.GetRootSignature());
            commandList->SetPipelineState(pipeline.GetPipelineState());
            break;
        }
    }
}
