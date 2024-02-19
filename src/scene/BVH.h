#pragma once

#include <vector>

#include "bbox.h"
#include "scene.h"

class BVHNode {
    private:
        BoundingBox boundingBox;
        // bool boundsInitialized;
        std::vector<Geometry*> objects;
        std::vector<BVHNode*> children;

    public:
        BVHNode(std::vector<Geometry*> objects) {
            // this->boundingBox = bBox;
            // this->boundsInitialized = false;
            // SplitNode also populates the boundingBox
            this->objects = objects;
            splitNode();
        }

        auto getChildren() {
            return this->children;
        }

        auto getBoundingBox() {
            // TODO: Should we move this to cpp instead?
            // if (!boundsInitialized) {
            //     this->boundingBox = BoundingBox();
            //     for (auto object : this->objects) {
            //         this->boundingBox.merge(object->getBoundingBox());
            //     }
            //     boundsInitialized = true;
            // }
            return this->boundingBox;
        }

        bool intersect(ray &r, isect &i) const;
        void splitNode();
};

class BVH {
    private:
        const Scene *scene;
        BVHNode *root;

        void buildBVH();

    public:
        BVH(const Scene *scene) {
            this->scene = scene;
            buildBVH();
        }

        bool intersect(ray &r, isect &i) const;
};