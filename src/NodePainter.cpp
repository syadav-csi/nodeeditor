#include "NodePainter.hpp"

#include <cmath>

#include <QtCore/QMargins>

#include "StyleCollection.hpp"
#include "PortType.hpp"
#include "NodeGraphicsObject.hpp"
#include "NodeGeometry.hpp"
#include "NodeState.hpp"
#include "NodeDataModel.hpp"
#include "Node.hpp"
#include "FlowScene.hpp"

using QtNodes::NodePainter;
using QtNodes::NodeGeometry;
using QtNodes::NodeGraphicsObject;
using QtNodes::Node;
using QtNodes::NodeState;
using QtNodes::NodeDataModel;
using QtNodes::FlowScene;

void
    NodePainter::
    paint(QPainter* painter,
          Node & node,
          FlowScene const& scene)
{
    NodeGeometry& geom = node.nodeGeometry();
    NodeState const& state = node.nodeState();
    NodeGraphicsObject const & graphicsObject = node.nodeGraphicsObject();

           // FORCE SQUARE SHAPE - width = height
    if(geom.width() != geom.height()) {
        double size = std::max(geom.width(), geom.height());
        geom.setWidth(size);
        geom.setHeight(size);
    }

    geom.recalculateSize(painter->font());

           //--------------------------------------------
    NodeDataModel const * model = node.nodeDataModel();

    drawNodeRect(painter, geom, model, graphicsObject);
    drawConnectionPoints(painter, geom, state, model, scene);
    drawFilledConnectionPoints(painter, geom, state, model);
    drawModelName(painter, geom, state, model);
    drawEntryLabels(painter, geom, state, model);
    drawResizeRect(painter, geom, model);
    drawValidationRect(painter, geom, model, graphicsObject);

           /// call custom painter
    if (auto painterDelegate = model->painterDelegate())
    {
        painterDelegate->paint(painter, geom, model);
    }
}


void
    NodePainter::
    drawNodeRect(QPainter* painter,
                 NodeGeometry const& geom,
                 NodeDataModel const* model,
                 NodeGraphicsObject const & graphicsObject)
{
    NodeStyle const& nodeStyle = model->nodeStyle();

    auto color = graphicsObject.isSelected()
                     ? nodeStyle.SelectedBoundaryColor
                     : nodeStyle.NormalBoundaryColor;

    if (geom.hovered())
    {
        QPen p(color, nodeStyle.HoveredPenWidth);
        painter->setPen(p);
    }
    else
    {
        QPen p(color, nodeStyle.PenWidth);
        painter->setPen(p);
    }

    // Draw blue background to show bounding rect (for debugging)
    // QBrush wBrush(Qt::blue);
    // QRect fullBoundary(geom.boundingRect().x(),
    //                    geom.boundingRect().y(),
    //                    geom.boundingRect().width(),
    //                    geom.boundingRect().height()
    //                     );
    // painter->fillRect(fullBoundary, wBrush);

           // Load the pixmap
    QPixmap pixmap;
    if(model->nodeType() == NT_WSR) {
        pixmap = QPixmap(":/images/plugFlowReactor.png");
    }
    else if(model->nodeType() == NT_PFR) {
        pixmap = QPixmap(":/images/7_general-flow-a.svg");
    }
    else {
        pixmap = QPixmap(":/images/reactor_intlet-01.png");
    }

    if (!pixmap.isNull()) {
        // CENTER THE PIXMAP in the bounding rect
        double nodeSize = geom.width(); // Square, so width = height

        // Calculate scaled pixmap size (maintain aspect ratio, fit within node)
        QSize pixmapSize = pixmap.size();
        pixmapSize.scale(nodeSize * 0.8, nodeSize * 0.8, Qt::KeepAspectRatio);

        //Calculate the area for node label
        QFont f = painter->font();
        f.setBold(true);
        QFontMetrics metrics(f);
        auto labelRect = metrics.boundingRect(model->caption());

        // Calculate top-left position to center the pixmap
        double x = (nodeSize - pixmapSize.width()) / 2.0;
        double y = geom.boundingRect().center().y() - (pixmapSize.height()/2) + labelRect.height();

        QRectF targetRect(x, y, pixmapSize.width(), pixmapSize.height());

        // Draw the centered pixmap
        painter->drawPixmap(targetRect.toRect(), pixmap);
    }
}


void
    NodePainter::
    drawConnectionPoints(QPainter* painter,
                         NodeGeometry const& geom,
                         NodeState const& state,
                         NodeDataModel const * model,
                         FlowScene const & scene)
{
    NodeStyle const& nodeStyle      = model->nodeStyle();
    auto const     &connectionStyle = StyleCollection::connectionStyle();

    float diameter = nodeStyle.ConnectionPointDiameter;
    auto  reducedDiameter = diameter * 0.5;

    for(PortType portType: {PortType::Out, PortType::In})
    {
        size_t n = state.getEntries(portType).size();

        for (unsigned int i = 0; i < n; ++i)
        {
            QPointF p = geom.portScenePosition(i, portType);
            auto const & dataType =
                model->dataType(portType, static_cast<int>(i));

            bool canConnect = (state.getEntries(portType)[i].empty() ||
                               (portType == PortType::Out &&
                                model->portOutConnectionPolicy(i) == NodeDataModel::ConnectionPolicy::Many) );

            double r = 1.0;
            if (state.isReacting() &&
               canConnect &&
               portType == state.reactingPortType())
            {

                auto   diff = geom.draggingPos() - p;
                double dist = std::sqrt(QPointF::dotProduct(diff, diff));
                bool   typeConvertable = false;

                {
                    if (portType == PortType::In)
                    {
                        typeConvertable = scene.registry().getTypeConverter(state.reactingDataType(), dataType) != nullptr;
                    }
                    else
                    {
                        typeConvertable = scene.registry().getTypeConverter(dataType, state.reactingDataType()) != nullptr;
                    }
                }

                if (state.reactingDataType().id == dataType.id || typeConvertable)
                {
                    double const thres = 40.0;
                    r = (dist < thres) ?
                            (2.0 - dist / thres ) :
                            1.0;
                }
                else
                {
                    double const thres = 80.0;
                    r = (dist < thres) ?
                            (dist / thres) :
                            1.0;
                }
            }

            if (connectionStyle.useDataDefinedColors())
            {
                painter->setBrush(connectionStyle.normalColor(dataType.id));
            }
            else
            {
                painter->setBrush(nodeStyle.ConnectionPointColor);
            }
            painter->drawEllipse(p,
                                 reducedDiameter * r,
                                 reducedDiameter * r);
        }
        if(model->nodeType() == 0) {
            break; //ignore InPort if inlet node type
        }
    };
}


void
    NodePainter::
    drawFilledConnectionPoints(QPainter * painter,
                               NodeGeometry const & geom,
                               NodeState const & state,
                               NodeDataModel const * model)
{
    NodeStyle const& nodeStyle       = model->nodeStyle();
    auto const     & connectionStyle = StyleCollection::connectionStyle();

    auto diameter = nodeStyle.ConnectionPointDiameter;


    for(PortType portType: {PortType::Out, PortType::In})
    {
        size_t n = state.getEntries(portType).size();

        for (size_t i = 0; i < n; ++i)
        {
            QPointF p = geom.portScenePosition(
                static_cast<PortIndex>(i),
                static_cast<PortType>(portType));

            if (!state.getEntries(portType)[i].empty())
            {
                auto const & dataType =
                    model->dataType(portType, static_cast<int>(i));

                if (connectionStyle.useDataDefinedColors())
                {
                    QColor const c = connectionStyle.normalColor(dataType.id);
                    painter->setPen(c);
                    painter->setBrush(c);
                }
                else
                {
                    painter->setPen(nodeStyle.FilledConnectionPointColor);
                    painter->setBrush(nodeStyle.FilledConnectionPointColor);
                }

                painter->drawEllipse(p,
                                     diameter * 0.4,
                                     diameter * 0.4);
            }
        }
    }
}


void
    NodePainter::
    drawModelName(QPainter * painter,
                  NodeGeometry const & geom,
                  NodeState const & state,
                  NodeDataModel const * model)
{
    NodeStyle const& nodeStyle = model->nodeStyle();

    Q_UNUSED(state);

    if (!model->captionVisible())
        return;

    QString const &name = model->caption();

    QFont f = painter->font();

    f.setBold(true);

    QFontMetrics metrics(f);

    auto rect = metrics.boundingRect(name);
    auto nodeRect = geom.boundingRect();

    QPointF position((geom.width() - rect.width()) / 2.0, nodeRect.y()+rect.height());

    painter->setFont(f);
    painter->setPen(nodeStyle.FontColor);
    if(model->nodeType() == NT_PFR) {
        painter->drawText(position, name);
    }
    else if(model->nodeType() == NT_WSR) {
        painter->drawText(position, name);
    }
    else {
        painter->drawText(position, name);
    }

    f.setBold(false);
    painter->setFont(f);
}


void
    NodePainter::
    drawEntryLabels(QPainter * painter,
                    NodeGeometry const & geom,
                    NodeState const & state,
                    NodeDataModel const * model)
{
    QFontMetrics const & metrics =
        painter->fontMetrics();

    for(PortType portType: {PortType::Out, PortType::In})
    {
        auto const &nodeStyle = model->nodeStyle();

        auto& entries = state.getEntries(portType);

        size_t n = entries.size();

        for (size_t i = 0; i < n; ++i)
        {
            QPointF p = geom.portScenePosition(static_cast<PortIndex>(i), portType);

            if (entries[i].empty())
                painter->setPen(nodeStyle.FontColorFaded);
            else
                painter->setPen(nodeStyle.FontColor);

            QString s;

            if (model->portCaptionVisible(portType, static_cast<PortIndex>(i)))
            {
                s = model->portCaption(portType, static_cast<PortIndex>(i));
            }
            else
            {
                s = model->dataType(portType, static_cast<int>(i)).name;
            }

            auto rect = metrics.boundingRect(s);

            p.setY(p.y() + rect.height() / 2.0);

            switch (portType)
            {
                case PortType::In:
                    p.setX(5.0);
                    break;

                case PortType::Out:
                    p.setX(geom.width() - 5.0 - rect.width());
                    break;

                default:
                    break;
            }

            painter->drawText(p, s);
        }
    }
}


void
    NodePainter::
    drawResizeRect(QPainter * painter,
                   NodeGeometry const & geom,
                   NodeDataModel const * model)
{
    if (model->resizable())
    {
        painter->setBrush(Qt::gray);
        painter->drawEllipse(geom.resizeRect());
    }
}


void
    NodePainter::
    drawValidationRect(QPainter * painter,
                       NodeGeometry const & geom,
                       NodeDataModel const * model,
                       NodeGraphicsObject const & graphicsObject)
{
    auto modelValidationState = model->validationState();

    if (modelValidationState != NodeValidationState::Valid)
    {
        NodeStyle const& nodeStyle = model->nodeStyle();

        auto color = graphicsObject.isSelected()
                         ? nodeStyle.SelectedBoundaryColor
                         : nodeStyle.NormalBoundaryColor;

        if (geom.hovered())
        {
            QPen p(color, nodeStyle.HoveredPenWidth);
            painter->setPen(p);
        }
        else
        {
            QPen p(color, nodeStyle.PenWidth);
            painter->setPen(p);
        }

               //Drawing the validation message background
        if (modelValidationState == NodeValidationState::Error)
        {
            painter->setBrush(nodeStyle.ErrorColor);
        }
        else
        {
            painter->setBrush(nodeStyle.WarningColor);
        }

        double const radius = 3.0;

               // Draw validation rect at the bottom within bounds
        QRectF boundary(0,
                        geom.height() - geom.validationHeight(),
                        geom.width(),
                        geom.validationHeight());

        painter->drawRoundedRect(boundary, radius, radius);

        painter->setBrush(Qt::gray);

               //Drawing the validation message itself
        QString const &errorMsg = model->validationMessage();

        QFont f = painter->font();
        QFontMetrics metrics(f);
        auto rect = metrics.boundingRect(errorMsg);

        QPointF position((geom.width() - rect.width()) / 2.0,
                         geom.height() - geom.validationHeight() / 2.0 + rect.height() / 2.0);

        painter->setFont(f);
        painter->setPen(nodeStyle.FontColor);
        painter->drawText(position, errorMsg);
    }
}


void NodePainter::resizeNodeByFactor(Node & node, double factor)
{
    NodeGeometry& geom = node.nodeGeometry();

    // Get current size
    double currentSize = geom.width(); // Square, so width = height

    // Calculate new size
    double newSize = currentSize * factor;

    // Apply new size (force square)
    geom.setWidth(newSize);
    geom.setHeight(newSize);

    // Trigger update
    node.nodeGraphicsObject().update();
}
