set(RASTERM_COMMON_SOURCES
    src/backend/sixel/SixelBackend.cpp
    src/capi/rasterm_c.cpp
    src/engine/Engine.cpp
    src/engine/Presenter.cpp
    src/diagnostics/DiagnosticReporter.cpp
    src/color/ColorConverter.cpp
    src/damage/DamageTracker.cpp
    src/encoder/sixel/SixelEncoder.cpp
    src/encoder/sixel/SixelPalette.cpp
    src/encoder/sixel/SixelSimdAvx2.cpp
    src/encoder/sixel/SixelSimdAvx512.cpp
    src/encoder/sixel/SixelWriter.cpp
    src/render/Renderer.cpp
    src/render/DamagePlanner.cpp
    src/scaling/ImageScaler.cpp
    src/output/StdoutSink.cpp
    src/output/TerminalRenderer.cpp
)

set(RASTERM_PLATFORM_SOURCES)
if(WIN32)
    list(APPEND RASTERM_PLATFORM_SOURCES
        src/platform/windows/ConsoleSession.cpp
        src/platform/windows/TerminalOwner.cpp
    )
endif()

set(RASTERM_CORE_SOURCES
    ${RASTERM_COMMON_SOURCES}
    ${RASTERM_PLATFORM_SOURCES}
)

set(RASTERM_SIXEL_AVX2_SOURCE src/encoder/sixel/SixelSimdAvx2.cpp)
set(RASTERM_SIXEL_AVX512_SOURCE src/encoder/sixel/SixelSimdAvx512.cpp)
