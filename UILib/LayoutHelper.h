/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2020-01-06
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#pragma once

#include <GameCore/GameTypes.h>
#include <GameCore/Log.h>

#include <algorithm>
#include <cassert>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

class LayoutHelper
{
public:

    template<typename TElement>
    struct LayoutElement
    {
        TElement Element;
        std::optional<IntegralCoordinates> Coordinates;

        LayoutElement(
            TElement const & element,
            std::optional<IntegralCoordinates> const & coordinates)
            : Element(element)
            , Coordinates(coordinates)
        {}
    };

    /*
     * Lays out elements in a grid. Accepts elements with or without fixed grid coordinates.
     * Expected coordinates:
     *  x = 0 is center, x = -1, -2, ... are on the left, x = +1, +2, ... are on the right
     *  y = 0 is top, y = +1, +2, ... are below
     */
    template<typename TElement>
    static void Layout(
        std::vector<LayoutElement<TElement>> layoutElements,
        int maxElementsPerRow,
        std::function<void(int width, int height)> const & onBegin,
        std::function<void(std::optional<TElement> element, int x, int y)> const & onPosition)
    {
        assert(maxElementsPerRow > 0);

        int allElementsCount = static_cast<int>(layoutElements.size());

        //
        // - Split elements;
        // - Calculate max x and y among decorated elements
        //

        std::vector<LayoutElement<TElement>> decoratedElements;
        std::vector<TElement> undecoratedElements;

        int maxDecoratedX = 0;
        int maxDecoratedY = 0;

        for (auto const & element : layoutElements)
        {
            if (!!(element.Coordinates))
            {
                maxDecoratedX = std::max(maxDecoratedX, abs(element.Coordinates->x));
                maxDecoratedY = std::max(maxDecoratedY, element.Coordinates->y);
                decoratedElements.emplace_back(element);
            }
            else
            {
                undecoratedElements.emplace_back(element.Element);
            }
        }

        //
        // Calculate bounding box for decorated elements only
        //

        int const decoratedWidth = decoratedElements.empty() ? 0 : maxDecoratedX * 2 + 1;
        int const decoratedHeight = decoratedElements.empty() ? 0 : maxDecoratedY + 1;

        //
        // Distribute surplus elements
        //

        int width = decoratedWidth;
        int height = decoratedHeight;

        int surplusCells = std::max(0, allElementsCount - width * height);

        // 1: Make sure there's at least room for one element
        if (surplusCells > 0
            && (width == 0 && height == 0))
        {
            width = 1;
            height = 1;

            // Distribute this one out
            surplusCells -= 1;
        }

        // 2: Make wider up to max width
        //  - As long as we don't have more than one row
        if (surplusCells > 0
            && (height == 0 || height == 1))
        {
            // Calculate number of cells we may grow horizontally on row 1
            int availableCells = std::max(0, maxElementsPerRow - width);
            int extraCols = std::min(surplusCells, availableCells);

            // Calculate additional number of columns now, making
            // sure we're symmetric wrt x=0
            int extraWidth = extraCols + (extraCols % 2);

            // Adjust width and surplus cell
            width += extraWidth;
            surplusCells = std::max(0, surplusCells - extraWidth);
        }

        // 3: Add a second row
        //  - As long as we have only one row
        if (surplusCells > 0
            && height == 1)
        {
            // Grow by one row
            height = 2;
            surplusCells = std::max(0, surplusCells - width);
        }

        // 4: Distribute vertically first, then horizontally
        if (surplusCells > 0)
        {
            assert(height > 0); // By now...

            // Distribute all remaining cells vertically, then horizontally
            int extraCols = surplusCells / height + ((surplusCells % height) != 0 ? 1 : 0);

            // Calculate additional number of columns now, making
            // sure we're symmetric wrt x=0
            int extraWidth = extraCols + (extraCols % 2);

            // Adjust width and surplus cell
            width += extraWidth;
            surplusCells = std::max(0, surplusCells - extraWidth * height);
        }

        assert(surplusCells == 0);

        LogMessage("Layout: decoratedW=", decoratedWidth, ", decoratedH=", decoratedHeight, ", W=", width, ", H=", height);

        //
        // Announce bounding box
        //

        onBegin(width, height);

        //
        // Sort decorated elements by y, x
        //

        std::sort(
            decoratedElements.begin(),
            decoratedElements.end(),
            [](auto const & lhs, auto const & rhs)
            {
                assert(!!(lhs.Coordinates));
                return lhs.Coordinates->y < rhs.Coordinates->y
                    || (lhs.Coordinates->y == rhs.Coordinates->y && lhs.Coordinates->x < rhs.Coordinates->x);
            });

        //
        // Position all items
        //

        auto decoratedIt = decoratedElements.cbegin();
        auto undecoratedIt = undecoratedElements.cbegin();

        for (int h = 0; h < height; ++h)
        {
            for (int w = 0; w < width; ++w)
            {
                int const col = w - width / 2;

                std::optional<TElement> positionElement;
                if (decoratedIt != decoratedElements.cend()
                    && h == decoratedIt->Coordinates->y
                    && col >= decoratedIt->Coordinates->x)
                {
                    // Position a decorated element
                    positionElement = decoratedIt->Element;
                    ++decoratedIt;
                }
                else if (undecoratedIt != undecoratedElements.cend())
                {
                    // Position an undecorated element
                    positionElement = *undecoratedIt;
                    ++undecoratedIt;
                }
                else
                {
                    // Position a spacer
                    positionElement = std::nullopt;
                }

                onPosition(
                    positionElement,
                    col,
                    h);
            }
        }

        assert(decoratedIt == decoratedElements.cend());
        assert(undecoratedIt == undecoratedElements.cend());
    }
};