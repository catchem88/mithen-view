#ifndef QVGRAPHICSVIEW_H
#define QVGRAPHICSVIEW_H

#include "qvnamespace.h"
#include "qvimagecore.h"
#include "qvocr.h"
#include "axislocker.h"
#include "logicalpixelfitter.h"
#include "scrollhelper.h"
#include <optional>
#include <QGraphicsView>
#include <QImageReader>
#include <QMimeData>
#include <QDir>
#include <QTimer>
#include <QProgressBar>
#include <QFileInfo>

class MainWindow;

class QVGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    QVGraphicsView(QWidget *parent = nullptr);

    struct SwipeData
    {
        int totalDelta;
        bool triggeredAction;
    };

    QMimeData* getMimeData() const;
    void loadMimeData(const QMimeData *mimeData);
    void loadFile(const QString &fileName, const QString &baseDir = "", std::optional<int> initialFrameNumber = {});

    void reloadFile();

    void zoomIn();

    void zoomOut();

    void zoomRelative(const qreal relativeLevel, const std::optional<QPoint> &mousePos = {});

    void zoomAbsolute(const qreal absoluteLevel, const std::optional<QPoint> &targetPos = {}, const bool isApplyingCalculation = false);

    const std::optional<Qv::CalculatedZoomMode> &getCalculatedZoomMode() const;
    void setCalculatedZoomMode(const std::optional<Qv::CalculatedZoomMode> &value, const bool isNavigating = false, const std::optional<QPoint> &mousePos = {});

    bool getNavigationResetsZoom() const { return navigationResetsZoom; }
    void setNavigationResetsZoom(const bool value);

    Qv::SortMode getSortMode() const { return imageCore.getSortMode(); }
    void setSortMode(const Qv::SortMode mode) { imageCore.setSortMode(mode); }
    bool getSortDescending() const { return imageCore.getSortDescending(); }
    void setSortDescending(const bool descending) { imageCore.setSortDescending(descending); }

    void resizeImage(const qreal factor) { imageCore.resizeImage(factor); }
    void cropImage(const QRect &rect) { imageCore.cropImage(rect); }
    bool getImageModified() const { return imageCore.getImageModified(); }
    void clearImageModified() { imageCore.clearImageModified(); }

    bool hasUnsavedTransform() const;
    QImage getCurrentTransformedImage() const;

    //True when the loaded file's format can be written by Qt
    bool canSaveTransformedImage() const;

    //Size of the image as currently displayed (transform applied)
    QSize getCurrentImageSize() const;

    void revertTransform();
    void startCrop();
    bool getIsCropping() const { return isCropping; }

    void startOcr();
    void finishOcr(const QList<QVOcrBox> &boxes);
    void cancelOcr();
    void showOcrToast(const QString &message,const int timeoutMs = 2000);
    bool getIsOcrRunning() const { return isOcrRunning; }
    bool getIsShowingOcr() const { return isShowingOcr; }
    QString getOcrText() const;

    void applyExpensiveScaling();
    void removeExpensiveScaling();

    void recalculateZoom();

    void centerImage();

    void scrollImage(int deltaX, int deltaY);

    void setCursorVisible(const bool visible);

    const QJsonObject getSessionState() const;

    void loadSessionState(const QJsonObject &state);

    void setLoadIsFromSessionRestore(const bool value);

    void setPendingMaximize(const bool value) { isPendingMaximize = value; }

    void setSmallImageMode(const bool value) { isSmallImageMode = value; }

    void goToFile(const Qv::GoToFileMode mode, const int index = 0);

    void settingsUpdated(const bool isInitialLoad);

    void closeImage(const bool stayInDir = false);
    void jumpToNextFrame();
    void jumpToPreviousFrame();
    void setPaused(const bool &desiredState);
    void setSpeed(const int &desiredSpeed);
    void rotateImage(const int relativeAngle);
    void mirrorImage();
    void flipImage();
    void resetTransformation();

    void fitOrConstrainImage();

    QSizeF getEffectiveOriginalSize() const;

    LogicalPixelFitter getPixelFitter() const;

    QRect getContentRect() const;

    QRect getImageViewportRect() const;

    const QVImageCore::FileDetails& getCurrentFileDetails() const { return imageCore.getCurrentFileDetails(); }
    const QVMovie& getLoadedMovie() const { return imageCore.getLoadedMovie(); }
    const QPixmap& getLoadedPixmap() const { return imageCore.getLoadedPixmap(); }
    bool hasFileOrPendingLoad() const { return imageCore.hasFileOrPendingLoad(); }
    qreal getZoomLevel() const { return zoomLevel; }
    int getFitOverscan() const { return fitOverscan; }
    Qv::CalculatedZoomMode getDefaultCalculatedZoomMode() const { return defaultCalculatedZoomMode; }

signals:
    void cancelSlideshow();

    void fileChanged(const bool isRestoringState);

    void zoomLevelChanged();

    void calculatedZoomModeChanged();

    void navigationResetsZoomChanged();

    //Emitted when the image transform or its pixel data changed
    void transformChanged();

    //Emitted when an interactive crop was applied
    void cropApplied();

    //Emitted when a recognized OCR line was clicked and its text should be copied
    void ocrBoxClicked(const QString &text);

    //Emitted when a recognition started or the results were shown/cleared
    void ocrStateChanged();

    void sortParametersChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;

    void dragLeaveEvent(QDragLeaveEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    bool event(QEvent *event) override;

    void focusInEvent(QFocusEvent *event) override;

    void focusOutEvent(QFocusEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *event) override;

    void executeClickAction(const Qv::ViewportClickAction action, const QPoint mousePos);

    void startDragAction(const Qv::ViewportDragAction action);

    void resetDragState();

    void executeDragAction(const Qv::ViewportDragAction action, const QPoint delta, bool &isMovingWindow);

    void executeScrollAction(const Qv::ViewportScrollAction action, const QPoint delta, const QPoint mousePos, const bool hasShiftModifier);

    bool isSmoothScalingRequested() const;

    bool isExpensiveScalingRequested() const;

    void matchContentCenter(const QRect target);

    std::optional<Qv::GoToFileMode> getNavigationRegion(const QPoint mousePos) const;

    QRect getUsableViewportRect(const bool addOverscan = false) const;

    void setTransformScale(const qreal absoluteScale);

    void setTransformWithNormalization(const QTransform &matrix);

    QTransform getUnspecializedTransform() const;

    QTransform normalizeTransformOrigin(const QTransform &matrix, const QSizeF &pixmapSize) const;

    qreal getDpiAdjustment() const;

    void handleDpiAdjustmentChange();

    void handleSmoothScalingChange();

    int getRtlFlip() const;

    void cancelTurboNav();

    MainWindow* getMainWindow() const;

private slots:
    void animatedFrameChanged(QRect rect);

    void beforeLoad();

    void postLoad();

    void imageChanged();

    void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    QGraphicsPixmapItem *loadedPixmapItem;

    Qv::SmoothScalingMode smoothScalingMode {Qv::SmoothScalingMode::Disabled};
    std::optional<qreal> smoothScalingLimit;
    bool expensiveScalingAboveWindowSize {false};
    std::optional<qreal> fitZoomLimit;
    int fitOverscan {0};
    bool zoomToCursor {true};
    bool useOneToOnePixelSizing {true};
    bool constrainImagePosition {true};
    bool constrainToCenterWhenSmaller {true};
    bool disableDelayedConstraint {false};
    Qv::CalculatedZoomMode defaultCalculatedZoomMode {Qv::CalculatedZoomMode::ZoomToFit};
    qreal zoomMultiplier {1.25};

    bool enableNavigationRegions {false};

    //Right-button drag gestures
    bool gestureNavigationEnabled {true};
    bool gestureZoomEnabled {false};
    bool isGestureDrag {false};
    bool gesturePerformed {false};
    QPoint gestureStartPos;
    qreal gestureStartZoomLevel {1.0};

    //Interactive crop
    enum class CropHandle { None, Move, Left, Top, Right, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };

    CropHandle getCropHandleAt(const QPoint &pos) const;
    void finishCrop();
    void cancelCrop();

    bool isCropping {false};
    QRect cropRect;
    CropHandle cropHandle {CropHandle::None};
    QPoint cropDragStartPos;
    QRect cropDragStartRect;

    //OCR results
    QRect getOcrBoxViewportRect(const QRect &imageRect) const;
    int getOcrBoxAt(const QPoint &pos) const;
    void updateOcrBusyIndicator();
    void drawOverlayToast(QPainter *painter,const QString &text) const;

    bool isOcrRunning {false};
    bool isShowingOcr {false};
    QList<QVOcrBox> ocrBoxes;
    int hoveredOcrBox {-1};
    QString ocrToastMessage;
    QTimer *ocrToastTimer {nullptr};
    QProgressBar *ocrBusyIndicator {nullptr};
    Qv::ViewportClickAction doubleClickAction {Qv::ViewportClickAction::None};
    Qv::ViewportClickAction altDoubleClickAction {Qv::ViewportClickAction::None};
    Qv::ViewportDragAction dragAction {Qv::ViewportDragAction::None};
    Qv::ViewportDragAction altDragAction {Qv::ViewportDragAction::None};
    Qv::ViewportClickAction middleClickAction {Qv::ViewportClickAction::None};
    Qv::ViewportClickAction altMiddleClickAction {Qv::ViewportClickAction::None};
    Qv::ClickOrDrag middleButtonMode {Qv::ClickOrDrag::Click};
    Qv::ViewportDragAction middleDragAction {Qv::ViewportDragAction::None};
    Qv::ViewportDragAction altMiddleDragAction {Qv::ViewportDragAction::None};
    Qv::ViewportScrollAction verticalScrollAction {Qv::ViewportScrollAction::None};
    Qv::ViewportScrollAction horizontalScrollAction {Qv::ViewportScrollAction::None};
    Qv::ViewportScrollAction altVerticalScrollAction {Qv::ViewportScrollAction::None};
    Qv::ViewportScrollAction altHorizontalScrollAction {Qv::ViewportScrollAction::None};
    bool scrollActionCooldown {false};

    std::optional<Qv::CalculatedZoomMode> calculatedZoomMode;
    bool globalNavigationResetsZoom {true};
    bool navigationResetsZoom {true};
    bool loadIsFromSessionRestore {false};
    qreal zoomLevel {1.0};
    qreal appliedDpiAdjustment {1.0};
    qreal appliedExpensiveScaleZoomLevel {0.0};
    std::optional<QPoint> lastZoomEventPos;
    QPointF lastZoomRoundingError;
    bool isCursorAutoHideFullscreenEnabled {true};
    bool isCursorVisible {true};
    QRect lastImageContentRect;

    // State for toggle original size - save previous zoom before switching to 100%
    std::optional<Qv::CalculatedZoomMode> previousCalculatedZoomMode;
    qreal previousZoomLevel {1.0};
    bool isOriginalSizeMode {false};
    QPoint previousScrollPos;

    // Flag to prevent postLoad from calculating zoom before window state change
    bool isPendingMaximize {false};
    // Flag for small images: window was sized to image, keep at 100% zoom
    bool isSmallImageMode {false};

    QVImageCore imageCore {this};

    QTimer *expensiveScaleTimer;
    QTimer *constrainBoundsTimer;
    QTimer *hideCursorTimer;

    ScrollHelper *scrollHelper;
    AxisLocker scrollAxisLocker;
    Qt::MouseButton pressedMouseButton {Qt::MouseButton::NoButton};
    Qt::KeyboardModifiers mousePressModifiers {Qt::KeyboardModifier::NoModifier};
    bool isDelayingDrag {false};
    bool isLastMousePosDubious {false};
    bool isSystemWindowDragActive {false};
    QPoint lastMousePos;
    QElapsedTimer lastFocusIn;

    std::optional<Qv::GoToFileMode> turboNavMode;
    QList<QKeySequence> navPrevShortcuts;
    QList<QKeySequence> navNextShortcuts;
    QList<QKeySequence> navRandomShortcuts;
    QElapsedTimer lastTurboNav;
    QElapsedTimer lastTurboNavKeyPress;
    int turboNavInterval {0};

    const int startDragDistance {3};
};
Q_DECLARE_METATYPE(QVGraphicsView::SwipeData)
#endif // QVGRAPHICSVIEW_H
