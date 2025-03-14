#include <algorithm>

#include "imgui_node_editor.h"
#include "node_graph.hpp"

SdfNodeEditor::SdfNodeEditor() {
  ed::Config config;
  editor = ed::CreateEditor(&config);
  output = new SurfaceOutputNode(nextId++);
  nodes.push_back(output);
}

SdfNodeEditor::~SdfNodeEditor() {
  ed::DestroyEditor(editor);
  for (Node* node : nodes)
    delete node;
}

void SdfNodeEditor::show() {
  ed::SetCurrentEditor(editor);
  ed::Begin("SDF Node Editor");

  for (auto* node : nodes) {
    ed::BeginNode(node->ID);
    node->draw();
    ed::EndNode();
  }

  for (auto& link : links)
    ed::Link(link.ID, link.StartPinID, link.EndPinID);

  manageCreation();
  manageDeletion();

  ed::End();
}

std::string SdfNodeEditor::generateGlslCode() const { return output->generateGlsl(); }

int SdfNodeEditor::getNextId() { return nextId++; }

Node* SdfNodeEditor::findNode(ed::NodeId id) {
  for (auto& node : nodes) {
    if (node->ID == id)
      return node;
  }
  return nullptr;
}

Pin* SdfNodeEditor::findPin(ed::PinId id) {
  if (!id)
    return nullptr;

  for (auto& node : nodes) {
    for (auto& pin : node->inputs) {
      if (pin.ID == id)
        return &pin;
    }
    for (auto& pin : node->outputs) {
      if (pin.ID == id)
        return &pin;
    }
  }

  return nullptr;
}

bool SdfNodeEditor::isInvalidPinLink(Pin* a, Pin* b) { //
  return (a == b || a->Kind == b->Kind || a->Type != b->Type || a->node == b->node);
}

bool SdfNodeEditor::linkExists(ed::PinId startID, ed::PinId endID) {
  return std::any_of(links.begin(), links.end(), [startID, endID](Link& l) { return l.StartPinID == startID && l.EndPinID == endID; });
}

void SdfNodeEditor::manageCreation() {
  if (ed::BeginCreate()) {
    ed::PinId startPinId = 0;
    ed::PinId endPinId = 0;

    if (ed::QueryNewLink(&startPinId, &endPinId)) {
      Pin* startPin = findPin(startPinId);
      Pin* endPin = findPin(endPinId);

      if (startPin != nullptr && endPin != nullptr) {
        if (isInvalidPinLink(startPin, endPin)) {
          ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
        } else if (ed::AcceptNewItem()) {
          if (endPin->Kind == PinKind::Input)
            std::swap(startPin, endPin);

          // TODO: Check for feedback loops
          if (!linkExists(startPinId, endPinId)) {
            startPin->pin = endPin;
            endPin->pin = startPin;
            links.emplace_back(getNextId(), startPin->ID, endPin->ID);
          }
        }
      }
    }
  }
  ed::EndCreate();
}

void SdfNodeEditor::manageDeletion() {
  if (ed::BeginDelete()) {
    // delete nodes
    ed::NodeId nodeId = 0;
    while (ed::QueryDeletedNode(&nodeId)) {
      if (!ed::AcceptDeletedItem())
        continue;

      // delete connected links
      for (int i = 0; i < links.size(); i++) {
        if (links[i].StartPinID.Get() == nodeId.Get() || links[i].EndPinID.Get() == nodeId.Get())
          links.erase(links.begin() + i);
      }

      // delete node
      for (int i = 1; i < nodes.size(); i++) {
        if (nodes[i]->ID == nodeId) {
          delete nodes[i];
          nodes.erase(nodes.begin() + i);
          break;
        }
      }
    }

    // delete links
    ed::LinkId linkId = 0;
    while (ed::QueryDeletedLink(&linkId)) {
      if (!ed::AcceptDeletedItem())
        continue;

      for (int i = 0; i < links.size(); i++) {
        if (links[i].ID == linkId) {
          Pin* start = findPin(links[i].StartPinID);
          Pin* end = findPin(links[i].EndPinID);
          if (start != nullptr)
            start->pin = nullptr;
          if (end != nullptr)
            end->pin = nullptr;
          links.erase(links.begin() + i);
          break;
        }
      }
    }
  }
  ed::EndDelete();
}
