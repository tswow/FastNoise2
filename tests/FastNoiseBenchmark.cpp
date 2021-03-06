#include <benchmark/benchmark.h>
#include "FastNoise/FastNoise.h"
#include "FastNoise/FastNoiseMetadata.h"

#include "magic_enum.h"

FastNoise::SmartNode<> BuildGenerator( benchmark::State& state, const FastNoise::Metadata* metadata, FastSIMD::eLevel level )
{    
    FastNoise::SmartNode<> generator = metadata->CreateNode( level );

    FastNoise::SmartNode<> source = FastNoise::New<FastNoise::Constant>( level );

    for( const auto& memberNode : metadata->memberNodes )
    {
        if( !memberNode.setFunc( generator.get(), source ) )
        {
            // If constant source is not valid try all other node types in order
            for( const FastNoise::Metadata* tryMetadata : FastNoise::Metadata::GetMetadataClasses() )
            {
                FastNoise::SmartNode<> trySource = tryMetadata->CreateNode( level );

                // Other node types may also have sources
                if( memberNode.setFunc( generator.get(), trySource ) )
                {
                    for( const auto& tryMemberNode : tryMetadata->memberNodes )
                    {
                        if( !tryMemberNode.setFunc( trySource.get(), source ) )
                        {
                            state.SkipWithError( "Could not set valid sources for generator" );
                            return {};
                        }                        
                    }
                    break;
                }
            }
        }
    }
    return generator;
}

void BenchFastNoiseGenerator2D( benchmark::State& state, int32_t testSize, const FastNoise::Metadata* metadata, FastSIMD::eLevel level )
{
    FastNoise::SmartNode<> generator = BuildGenerator( state, metadata, level );
    if (!generator) return;

    size_t dataSize = (size_t)testSize * testSize;

    float* data = new float[dataSize];
    size_t totalData = 0;
    int seed = 0;

    for( auto _ : state )
    {
        (void)_;
        generator->GenUniformGrid2D( data, 0, 0, testSize, testSize, 0.1f, seed++ );
        totalData += dataSize;
    }

    delete[] data;
    state.SetItemsProcessed( totalData );
}

void BenchFastNoiseGenerator3D( benchmark::State& state, int32_t testSize, const FastNoise::Metadata* metadata, FastSIMD::eLevel level )
{
    FastNoise::SmartNode<> generator = BuildGenerator( state, metadata, level );
    if (!generator) return;

    size_t dataSize = (size_t)testSize * testSize * testSize;

    float* data = new float[dataSize];
    size_t totalData = 0;
    int seed = 0;

    for( auto _ : state )
    {
        (void)_;
        generator->GenUniformGrid3D( data, 0, 0, 0, testSize, testSize, testSize, 0.1f, seed++ );
        totalData += dataSize;
    }

    delete[] data;
    state.SetItemsProcessed( totalData );
}

int main( int argc, char** argv )
{
    benchmark::Initialize( &argc, argv );
    
    for( FastSIMD::eLevel level = FastSIMD::CPUMaxSIMDLevel(); level != FastSIMD::Level_Null; level = (FastSIMD::eLevel)(level >> 1) )
    {
        if( !(level & FastSIMD::COMPILED_SIMD_LEVELS & FastNoise::SUPPORTED_SIMD_LEVELS) )
        {
            continue;
        }

        for( const FastNoise::Metadata* metadata : FastNoise::Metadata::GetMetadataClasses() )
        {
            std::string benchName = "2D/";

#ifdef MAGIC_ENUM_SUPPORTED
            auto enumName = magic_enum::flags::enum_name( level );
            auto find = enumName.find( '_' );
            if( find != std::string::npos )
            {
                benchName += enumName.data() + find + 1;                
            }
            else
            {
                benchName += enumName;     
            }
#else
            benchName += std::to_string( (int)level );
#endif

            const char* groupName = "None";

            if( !metadata->groups.empty() )
            {
                groupName = metadata->groups[metadata->groups.size() - 1];
            }

            benchName += '/';
            benchName += groupName;
            benchName += '/';
            benchName += FastNoise::Metadata::FormatMetadataNodeName( metadata, false );

            benchmark::RegisterBenchmark( benchName.c_str(), [=]( benchmark::State& st ) { BenchFastNoiseGenerator2D( st, 512, metadata, level ); } );

            benchName[0] = '3';

            benchmark::RegisterBenchmark( benchName.c_str(), [=]( benchmark::State& st ) { BenchFastNoiseGenerator3D( st, 64, metadata, level ); } );        
        }
    }

    benchmark::RunSpecifiedBenchmarks();

    return 0;
}