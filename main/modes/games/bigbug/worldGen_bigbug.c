//==============================================================================
// Includes
//==============================================================================
#include "tilemap_bigbug.h"
#include "entityManager_bigbug.h"
#include "entity_bigbug.h"
#include "worldGen_bigbug.h"
#include "random_bigbug.h"
#include "typedef_bigbug.h"

//==============================================================================
// Functions
//==============================================================================
void bb_initializeEggs(bb_entityManager_t* entityManager, bb_tilemap_t* tilemap)
{
    printf("initializing eggs\n");

    tilemap->fgTiles[TILE_FIELD_WIDTH/2 + 2][0].embed = EGG_EMBED;//tutorial egg

    for (int x = 21; x < TILE_FIELD_WIDTH - 21; x++)
    {
        for (int y = 4; y < TILE_FIELD_HEIGHT; y++)
        {
            if (tilemap->fgTiles[x][y].health > 0 && bb_randomInt(0, 1) == 0) //%50 chance
            {
                // printf("egg at i:%d j:%d\n", x, y);
                tilemap->fgTiles[x][y].embed = EGG_EMBED;

                // bb_entity_t* eggLeaves = bb_createEntity(entityManager, NO_ANIMATION, true, EGG_LEAVES, 1,
                //                                          x * TILE_SIZE + HALF_TILE, y * TILE_SIZE + HALF_TILE);
                // if(eggLeaves == NULL)
                // {
                //     //no more room in the EntityManager. :(
                //     return;
                // }
                // ((bb_eggLeavesData_t*)eggLeaves->data)->egg = bb_createEntity(
                //     entityManager, NO_ANIMATION, true, EGG, 1, x * TILE_SIZE + HALF_TILE, y * TILE_SIZE + HALF_TILE);
                // if(((bb_eggLeavesData_t*)eggLeaves->data)->egg == NULL)
                // {
                //     bb_destroyEntity(eggLeaves, false);
                // }
            }

            else
            {
                tilemap->fgTiles[x][y].embed = NOTHING_EMBED;
            }
        }
    }
}