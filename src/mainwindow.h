#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSizePolicy>
#include <QScrollArea>
#include <QTimer>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDataStream>
#include <QByteArray>
#include <QNetworkInterface>
#include <QSignalMapper>
#include "WindowsAudioVolumeController.h"

#define SAFE_DELETE(x) if(x != NULL){delete x; x = NULL;}

enum E_PacketType{pT_audioSession = 0, pT_resetAudioSessions = 1};

struct S_AudioSession_Network
{
    uint8_t packetType;
    uint8_t index;
    char displayName[128];
    uint8_t currentVolumeLevel;
    bool isMuted;

};

struct S_AudioSession_Network_Receive
{
    uint8_t index;
    char displayName[128];
    uint8_t currentVolumeLevel;
    bool isMuted;

};

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void Update();

    void SliderMoved(int index);
    void SpinBoxChanged(int index);

    void SliderReleased(int index);

    void NewClientConnection();
    void ClientDisconnected();
    void ReceivedClientPackage();


private:
    Ui::MainWindow *ui;

    QWidget* parentWidget;
    QGridLayout* parentLayout;

    QGridLayout* itemLayout;
    QScrollArea* scrollArea;
    QWidget* scrollAreaContent;

    QTimer* updateTimer;

    std::vector<QLabel*> labels;
    std::vector<QSlider*> slider;
    std::vector<QSpinBox*> spinBoxes;

    C_WindowsAudioVolumeController audioController;
    std::vector<S_AudioSession> audioSessions;

    QSignalMapper* mapper;
    QSignalMapper* mapper2;
    QSignalMapper* mapper3;

    QTimer* timer_receive;
    QTcpSocket* socket;
    QTcpServer* server;

    QLabel* labelIsConnected;
    QLabel* labelLocalIP;

    //FUNCTIONS

    void UpdateSessions();

    void InitializeUI();

    void SendAudioSessions();

    void SendAudioSession(int index);


};

inline void MainWindow::Update()
{
    UpdateSessions();
    updateTimer->start(100);
}

#endif // MAINWINDOW_H
