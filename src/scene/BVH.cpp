#include <glm/vec3.hpp>
#include <vector>
#include <iostream>
#include <stdio.h>

#include "BVH.h"
#include "scene.h"
#include "../SceneObjects/trimesh.h"

void BVH::buildBVH() {
    // BoundingBox sceneBox = scene->getBoundingBox();
    // std::cout<<"BVH.cpp BVH::buildBVH start\n";
    std::vector<Geometry*> sceneObjects;

    // TODO: Add objects - sort beforehand??
    // Actually don't sort yet, longest axis can keep changing for each split,
    // changing the sorting order. Sort on the go.
    // Can we do partial sorting? Maybe?

    for (auto objIter = scene->beginObjects(); objIter < scene->endObjects(); objIter++) {
        Geometry* obj = *objIter; // TODO: Check if this is the correct way.
        if (typeid(*obj) == typeid(Trimesh)) {
            // It's a trimesh. We treat each trimesh face as individual object.
            Trimesh* trimeshObj = (Trimesh*) obj;
            for (auto faceIter = trimeshObj->beginFaces(); faceIter < trimeshObj->endFaces(); faceIter++) {
                (*faceIter)->ComputeBoundingBox();
                sceneObjects.push_back((*faceIter));
            }
        }
        else {
            // obj->ComputeBoundingBox();
            sceneObjects.push_back(obj);
        }
    }

    root = new BVHNode(sceneObjects);

    // std::cout<<"BVH.cpp BVH::buildBVH end\n";

    // TODO: Split till leaves now. Recursive? - We let the constructor do this itself.
}

bool BVH::intersect(ray &r, isect &i) const {
    isect cur;
    bool have_one = root->intersect(r, cur);
    i = cur;
    return have_one;
}

void BVHNode::splitNode() { // For top-down BVH construction
    // Process to split a node:
    // 1.Find the longest axis
    // 2. Divide along that axis
    // 3. Construct bounding box by merging children bounding boxes -- design choice here.
    // Logically, we are splitting boxes here (top-down), but code efficiency wise, it is
    // better to merge than split

    // Basic checks for 1 or 2 objects?
    // TODO: should we also simplify in case of 2 objects to avoid the overhead of decisions?
    // We wanna always simply split for 2 objects

    // std::cout<<"BVH.cpp BVHNode::splitNode start\n";
    // std::cout<<"BVH.cpp BVHNode::splitNode "<<typeid(*(this->objects[0])).name()<<"\n";
    int numObjects = objects.size();

    // std::cout<<numObjects<<"\n";

    if (numObjects == 1) {
        this->boundingBox = BoundingBox();
        // std::cout<<"BVH.cpp BVHNode::splitNode "<<objects[0]->getBoundingBox().isEmpty()<<" Empty\n";
        (this->boundingBox).merge(objects[0]->getBoundingBox());
        return;
    }

    // 1.Find the longest axis
    glm::dvec3 boxMax = boundingBox.getMax();
    glm::dvec3 boxMin = boundingBox.getMin();
    glm::dvec3 axisLengths = boxMax - boxMin;

    int argMax = 0, max = axisLengths[0];
    for (int i=1; i<3; i++) {
        if (axisLengths[i] > max) {
            max = axisLengths[i];
            argMax = i;
        }
    }

    // 2. Divide along the axis - need to decide how to divide 
    // TODO: Currently does simple equal object division, see if can include SAH logic here

    // We need to find the correct middle object to split across.
    // We sort by object centers, which we approximate by bounding box center.


    std::nth_element(objects.begin(), objects.begin() + numObjects/2 , objects.end(),
                        [argMax](Geometry *a, Geometry *b) {
                            return a->getBoundingBox().getCenter()[argMax] < b->getBoundingBox().getCenter()[argMax];
                            }
                        );

    // Now, the center of bounding box for object at index s/2 is greater than the centers of all bounding box of objects
    // to its left in the list. (and same for the other direction) in its value on the argMax axis.

    std::vector<Geometry*> firstHalfObjects(objects.begin(), objects.begin() + numObjects/2);
    std::vector<Geometry*> secondHalfObjects(objects.begin() + numObjects/2, objects.end());
    BVHNode *first = new BVHNode(firstHalfObjects);
    BVHNode *second = new BVHNode(secondHalfObjects);

    children.push_back(first);
    children.push_back(second);

    // 3. Construct bounding box by merging children bounding boxes
    this->boundingBox = BoundingBox();
    (this->boundingBox).merge(first->getBoundingBox());
    (this->boundingBox).merge(second->getBoundingBox());

    // std::cout<<"BVH.cpp BVHNode::splitNode end\n";
    return;
}

bool BVHNode::intersect(ray &r, isect &i) const {
    // std::cout<<"BVH.cpp BVHNode::intersect "<<"Here1\n";
    double tmin = 0.0;
    double tmax = 0.0;
    bool have_one = false;


    // std::cout<<"BVH.cpp BVHNode::intersect "<<this->objects.size()<<"\n";
    // std::cout<<"BVH.cpp BVHNode::intersect "<<typeid(*(this->objects[0])).name()<<"\n";
    // std::cout<<"BVH.cpp BVHNode::intersect "<<objects[0]->getBoundingBox().isEmpty()<<" Empty\n";

    bool intersect = boundingBox.intersect(r, tmin, tmax);
    // std::cout<<"BVH.cpp BVHNode::intersect "<<intersect<<"\n";
    if (intersect) {
        // std::cout<<"BVH.cpp BVHNode::intersect "<<"Here\n";
        if (children.empty()) {
            // We are at the leaf node. Do actual object intersection here
            for (const auto &obj : objects) {
                isect cur;
                if (obj->intersect(r, cur)) {
                    if (!have_one || (cur.getT() < i.getT())) {
                        i = cur;
                        have_one = true;
                    }
                }
            }
        }
        else {
            for (const auto &child: children) {
                isect cur;
                if (child->intersect(r, cur)) {
                    if (!have_one || (cur.getT() < i.getT())) {
                        i = cur;
                        have_one = true;
                    }
                }
            }
        }
    }

    if (!have_one)
        i.setT(1000.0);
    // if debugging,
    // if (TraceUI::m_debug) {
    //     addToIntersectCache(std::make_pair(new ray(r), new isect(i)));
    // }
    return have_one;
}