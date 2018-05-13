#include <QMessageBox>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QStackedLayout>
#include <QTextBrowser>
#include <QLineEdit>
#include <QScrollBar>
#include <QColorDialog>
#include <QMenu>
#include <QFileDialog>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "accountinfodialog.h"
#include "systraynotification.h"
#include "emoticonchooser.h"
#include "global_objects.h"
#include "account.h"
#include "conversation.h"
#include "wichatconfig.h"

#define WICHAT_MAIN_FRILIST_FIELD_ICON 0
#define WICHAT_MAIN_FRILIST_FIELD_ICONPATH 1
#define WICHAT_MAIN_FRILIST_FIELD_ID 2

#define WICHAT_MAIN_FONT_STYLE_BOLD "bold"
#define WICHAT_MAIN_FONT_STYLE_ITALIC "italic"
#define WICHAT_MAIN_FONT_STYLE_UNDERLINE "underline"
#define WICHAT_MAIN_FONT_STYLE_STRIKEOUT "strikeout"
#define WICHAT_MAIN_FONT_STYLE_OVERLINE "overline"

#define WICHAT_MAIN_TEXT_ALIGN_LEFT "left"
#define WICHAT_MAIN_TEXT_ALIGN_CENTER "center"
#define WICHAT_MAIN_TEXT_ALIGN_RIGHT "right"

#define WICHAT_MAIN_EDITOR_TEXTBOX_NAME "tC"
#define WICHAT_MAIN_EDITOR_SENDKEY_ENTER 0x01000004
#define WICHAT_MAIN_EDITOR_SENDKEY_CTRLENTER 0x01000004 + 0x04000000

#define WICHAT_MAIN_RESOURCE_DIR "/usr/share"

#define WICHAT_MAIN_MENU_FRIEND_OPEN "open"
#define WICHAT_MAIN_MENU_FRIEND_REMOVE "remove"
#define WICHAT_MAIN_MENU_FRIEND_INFO "info"

#define WICHAT_MAIN_FILE_FILTER_ALL "All (*.*)(*.*)"
#define WICHAT_MAIN_FILE_FILTER_GIF "GIF file (*.gif)(*.gif)"
#define WICHAT_MAIN_FILE_FILTER_JPG "JPEG file (*.jpg *.jpeg)(*.jpg *.jpeg)"
#define WICHAT_MAIN_FILE_FILTER_PNG "PNG file (*.png)(*.png)"

#define WICHAT_MAIN_TIME_FORMAT "yyyy-MM-dd hh:mm:ss"

#define WICHAT_MAIN_TIMER_INTERVAL_GETFRIEND 30
#define WICHAT_MAIN_TIMER_INTERVAL_GETMSG 10
#ifdef QT_DEBUG
#define WICHAT_MAIN_TIMER_INTERVAL_SHOWNOTE 10
#else
#define WICHAT_MAIN_TIMER_INTERVAL_SHOWNOTE 2
#endif

Account::OnlineState Wichat_stateList[4] = {
        Account::OnlineState::Online,
        Account::OnlineState::Busy,
        Account::OnlineState::Hide,
        Account::OnlineState::Offline
};

QString Wichat_stateToString(Account::OnlineState state)
{
    QString stateString;

    switch (state)
    {
        case Account::OnlineState::Busy:
            stateString = "Busy";
            break;
        case Account::OnlineState::Hide:
            stateString = "Hide";
            break;
        case Account::OnlineState::Online:
            stateString = "Online";
            break;
        case Account::OnlineState::Offline:
        default:
            stateString = "Offline";
    }
    return stateString;
}

void Wichat_addHTMLHeader(QByteArray& buffer, int docType)
{
    // docType: 0=Content Window; 1=Input Window
    buffer.append("<!DOCTYPE html PUBLIC ""-//W3C//DTD HTML 4.01 Transitional//EN"" ""http://www.w3.org/TR/html4/loose.dtd""><html xmlns=""http://www.w3.org/1999/xhtml""><head><meta http-equiv=""Content-Type"" content=""text/html; charset=utf-8"" /><title></title>");
    switch (docType)
    {
        case 0:
            buffer.append("<style>div,span,ul,li{marin:0px;padding:0px;list-style-type:none;font-size:15px}p{text-align:left}img{border:0px;max-width:100%;height:auto}.s{color:#339966}.r{color:#0066CC}.a{color:#000000}.c{margin:0px 0px 15px 5px;width:350px;word-wrap:break-word}</style></head><body style=""width:360px;overflow:hidden;"">");
            break;
        case 1:
            buffer.append("<style>div,span,ul,li{marin:0px;padding:0px;list-style-type:none}p{text-align:left;}</style></head><body style=""width:98%;margin:2px;overflow:hidden""><textarea type=text id=" WICHAT_MAIN_EDITOR_TEXTBOX_NAME " cols=38 rows=3 style=""width:100%;overflow:hidden;border:0px;"">");
            break;
        default:;
    }
}

void Wichat_addHTMLFooter(QByteArray& buffer,int docType)
{
    switch (docType)
    {
        case 0:
            buffer.append("</body></html>");
            break;
        case 1:
            buffer.append("</textarea></body></html>");
            break;
        default:;
    }
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->textBrowser->hide();
    ui->textEdit->hide();
    ui->labelFriendSearch->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->tabSession->removeTab(0);
    ui->frameFont->hide();
    ui->frameMsg->hide();
    ui->frameTextGroup->setLayout(new QStackedLayout);
    ui->frameTextGroup->layout()->setMargin(0);
    ui->listFriend->setModel(&listFriendModel);

    menuFriendOption = new QMenu(ui->listFriend);
    menuSysTray = new QMenu();
    sysTrayIcon = new QSystemTrayIcon(this);
    sysTrayIcon->setContextMenu(menuSysTray);
    sysTrayNoteList = new SystrayNotification;
    emoticonList = new EmoticonChooser(this);
    emoticonList->hide();
    buttonTabClose = new QPushButton(QIcon(":/Icons/remove.ico"),
                                     "");
    buttonTabClose->setGeometry(0, 0, 10, 10);
    tabBarSession = ui->tabSession->findChild<QTabBar*>();
    listFriendModel.setColumnCount(3);

    connect(&globalAccount,
            SIGNAL(resetSessionFinished(int, bool)),
            this,
            SLOT(onChangeSessionFinished(int, bool)));
    connect(&globalAccount,
            SIGNAL(setStateFinished(int, bool, OnlineState)),
            this,
            SLOT(onChangeStateFinished(int,bool, OnlineState)));
    connect(&globalAccount,
            SIGNAL(queryFriendListFinished(int,
                                           QList<AccountListEntry>)),
            this,
            SLOT(onUpdateFriendListFinished(int,
                                            QList<AccountListEntry>)));
    connect(&globalAccount,
            SIGNAL(addFriendFinished(int, bool)),
            this,
            SLOT(onAddFriendFinished(int, bool)));
    connect(&globalAccount,
            SIGNAL(removeFriendFinished(int, bool)),
            this,
            SLOT(onRemoveFriendFinished(int, bool)));
    connect(&globalAccount,
            SIGNAL(queryFriendInfoFinished(int,
                                           QList<AccountInfoEntry>)),
            this,
            SLOT(onGetFriendInfoFinished(int,
                                         QList<AccountInfoEntry>)));
    connect(&globalAccount,
            SIGNAL(friendRequest(QString)),
            this,
            SLOT(onFriendRequest(QString)));
    connect(&globalAccount,
            SIGNAL(friendRemoved(QString)),
            this,
            SLOT(onFriendRemoved(QString)));
    connect(&globalConversation,
            SIGNAL(connectionBroken(QString)),
            this,
            SLOT(onConnectionBroken(QString)));
    connect(&globalConversation,
            SIGNAL(verifyFinished(int, Conversation::VerifyError)),
            this,
            SLOT(onConversationVerifyFinished(int, Conversation::VerifyError)));
    connect(&globalConversation,
            SIGNAL(resetSessionFinished(int, bool)),
            this,
            SLOT(onResetSessionFinished(int, bool)));
    connect(&globalConversation,
            SIGNAL(sendMessageFinished(int, bool)),
            this,
            SLOT(onSendMessageFinished(int, bool)));
    connect(&globalConversation,
            SIGNAL(getMessageListFinished(int,
                                          QList<MessageListEntry>)),
            this,
            SLOT(onGetMessageListFinished(int,
                                          QList<MessageListEntry>)));
    connect(&globalConversation,
            SIGNAL(receiveMessageFinished(int, QByteArray&)),
            this,
            SLOT(onReceiveMessageFinished(int, QByteArray&)));
    connect(buttonTabClose,
            SIGNAL(clicked(bool)),
            this,
            SLOT(onSessionTabClose(bool)));
    connect(menuFriendOption,
            SIGNAL(triggered(QAction*)),
            this,
            SLOT(onListFriendMenuClicked(QAction*)));
    connect(sysTrayIcon,
            SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this,
            SLOT(onSysTrayIconClicked(QSystemTrayIcon::ActivationReason)));
    connect(sysTrayNoteList,
            SIGNAL(noteClicked(const Notification::Note&)),
            this,
            SLOT(onSysTrayNoteClicked(const Notification::Note&)));
    connect(emoticonList,
            SIGNAL(emoticonClicked(QByteArray)),
            this,
            SLOT(onEmoticonClicked(QByteArray)));
    connect(menuSysTray,
            SIGNAL(aboutToShow()),
            this,
            SLOT(onSysTrayMenuShowed()));
    connect(menuSysTray,
            SIGNAL(triggered(QAction*)),
            this,
            SLOT(onSysTrayMenuClicked(QAction*)));
    connect(&timer,
            SIGNAL(timeout()),
            this,
            SLOT(onTimerTimeout()));

    installEventFilter(this);
    ui->listFriend->installEventFilter(this);
    tabBarSession->installEventFilter(this);

    accountInfo = nullptr;
    dialogColor = nullptr;
    menuFontStyle = nullptr;
    menuTextAlign = nullptr;
    menuSendOption = nullptr;
    groupTextAlign = nullptr;
    groupSendOption = nullptr;
    timer.setInterval(1000);
    manualExit = false;
    lastHoveredTab = 0;
    conversationLoginState = 0;
    notificationState = 0;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init()
{
    static bool hasInited = false;
    if (hasInited)
        return;

    userID = globalAccount.ID();

    userSessionList.loadFromFile(globalConfig.userDirectory(userID));
    if (userSessionList.count() < 1)
        loadSession(userID);
    else
    {
        loadTab();
        loadSession(userSessionList.currentSession().ID);
    }
    applyUserSettings();

    globalConversation.setPeerSession(peerSessionList);
    globalConversation.setUserDirectory(globalConfig.userDirectory(userID));

    resizeEvent(0); // Trigger resizing manually
    sysTrayIcon->show();
    emoticonList->setResourceDir(QDir::current()
                                 .absoluteFilePath(WICHAT_MAIN_RESOURCE_DIR));

    addTask(taskUpdateAll);
    timer.start();

    hasInited = true;
}

void MainWindow::setID(QString ID)
{
    userID = ID;
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        if (isMaximized())
            updateCaption();
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!manualExit)
    {
        if (QMessageBox::warning(this, "Exit Wichat",
                                 "Sure to exit WiChat?",
                                 QMessageBox::Yes | QMessageBox::No)
            != QMessageBox::Yes)
        {
            event->ignore();
            return;
        }
    }
    if (accountInfo)
        accountInfo->close();
    sysTrayIcon->hide();
    sysTrayNoteList->close();
    emoticonList->hide();

    syncSessionContent("");
    userSessionList.saveToFile(globalConfig.userDirectory(userID));
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::MouseButtonRelease:
            onMouseButtonRelease();
            break;
        case QEvent::HoverEnter:
            onMouseHoverEnter(watched, (QHoverEvent*)(event));
            break;
        case QEvent::HoverLeave:
            onMouseHoverLeave(watched, (QHoverEvent*)(event));
            break;
        case QEvent::KeyRelease:
            onKeyRelease(watched, (QKeyEvent*)(event));
            break;
        default:;
    }
    return QObject::eventFilter(watched, event);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);
    ui->tabSession->resize(ui->tabSession->width(),
                           ui->frameTextCtrl->y() - ui->tabSession->y());
    ui->textFriendSearch->resize(ui->frameFriendSearch->width(),
                                 ui->textFriendSearch->height());
    ui->labelFriendSearch->resize(ui->textFriendSearch->width() - 5,
                                  ui->labelFriendSearch->height());

}

void MainWindow::showEvent(QShowEvent * event)
{
    Q_UNUSED(event)
    init();
}

void MainWindow::addTask(TaskType task)
{
    taskList.enqueue(task);
}

void MainWindow::doTask()
{
    if (taskList.isEmpty())
        return;

    TaskType task = taskList.dequeue();
    switch (task)
    {
        case taskChangeSession:
            changeSession();
            break;
        case taskUpdateState:
            updateState();
            refreshTab();
        break;
        case taskShowNotification:
            showNotification();
            break;
        case taskUpdateFriList:
            updateFriendList();
            refreshTab();
            break;
        case taskUpdateAll:
            addTask(taskUpdateState);
            addTask(taskUpdateFriList);
            addTask(taskGetMsgList);
            break;
        case taskGetMsgList:
            getMessageList();
            break;
        case taskReloadMsg:
            loadSessionContent(lastConversation);
            break;
        case taskRebuildConnection:
            fixBrokenConnection();
            break;
        case taskConversationLogin:
            conversationLogin();
            break;
        default:;
    }
}

void MainWindow::applyFont()
{
    QFont font;
    QString temp;
    temp = globalConfig.prefFontStyle(userID);
    if (temp.contains(WICHAT_MAIN_FONT_STYLE_BOLD))
        font.setBold(true);
    if (temp.contains(WICHAT_MAIN_FONT_STYLE_ITALIC))
        font.setItalic(true);
    if (temp.contains(WICHAT_MAIN_FONT_STYLE_UNDERLINE))
        font.setUnderline(true);
    if (temp.contains(WICHAT_MAIN_FONT_STYLE_STRIKEOUT))
        font.setStrikeOut(true);
    if (temp.contains(WICHAT_MAIN_FONT_STYLE_OVERLINE))
        font.setOverline(true);
    font.setFamily(globalConfig.prefFontFamily(userID));
    font.setPointSize(globalConfig.prefFontSize(userID).toInt());
    for (int i=0; i<editorList.count(); i++)
    {
        editorList[i]->setFont(font);
        editorList[i]->setTextColor(globalConfig.prefFontColor(userID));
        temp = globalConfig.prefTextAlign(userID);
        if (temp == WICHAT_MAIN_TEXT_ALIGN_CENTER)
            editorList[i]->setAlignment(Qt::AlignCenter);
        else if (temp == WICHAT_MAIN_TEXT_ALIGN_RIGHT)
            editorList[i]->setAlignment(Qt::AlignRight);
        else
            editorList[i]->setAlignment(Qt::AlignLeft);
    }
}

void MainWindow::applyUserSettings()
{
    //applyFont();
}

int MainWindow::getSessionIndex(QString ID)
{
    for (int i=0; i<browserList.count(); i++)
    {
        if (browserList[i]->property("ID").toString() == ID)
            return i;
    }
    return -1;
}

int MainWindow::getSessionTabIndex(QString ID)
{
    for (int i=0; i<ui->tabSession->count(); i++)
    {
        if (((QTextBrowser*)(ui->tabSession->widget(i)))
                                        ->property("ID").toString() == ID)
            return i;
    }
    return -1;
}

// Load new session, and make it visible to user
void MainWindow::loadSession(QString ID, bool setTabActive)
{
    if (ID.isEmpty())
        return;

    int index = getSessionTabIndex(ID);
    if (index == -1)
    {
        // Create new session
        if (!userSessionList.exists(ID))
            userSessionList.add(ID);
        addTab(ID);
    }

    // Clear possible notifications of in-coming messages
    bool receivedMsg = false;
    QList<Notification::Note> notes = noteList.getAll();
    for (int i=0; i<notes.count(); i++)
    {
        if (notes[i].source == ID && notes[i].type == Notification::GotMsg)
        {
            noteList.remove(notes[i].ID);
            receivedMsg = true;
        }
    }
    if (receivedMsg && ID != userID)
    {
        int queryID;
        globalConversation.receiveMessage(ID, queryID);
        queryList[queryID] = ID;
    }

    index = getSessionTabIndex(ID);
    if (setTabActive)
    {
        ui->tabSession->setCurrentIndex(index);
        return;
    }
    highlightSession(ID, false);

    if (lastConversation == ID)
        return;
    UserSession::SessionData& oldSession =
                                userSessionList.getSession(lastConversation);
    if (!oldSession.ID.isEmpty())
    {
        int oldIndex = getSessionIndex(oldSession.ID);
        if (oldIndex >= 0)// && oldIndex != ui->tabSession->currentIndex())
        {
            syncSessionContent(oldSession.ID);
        }
    }

    index = getSessionIndex(ID);
    if (!browserList[index]->property("Initialized").toBool())
    {
        loadSessionContent(ID);
        browserList[index]->setProperty("Initialized", true);
    }
    ((QStackedLayout*)(ui->frameTextGroup->layout()))->setCurrentIndex(index);
    lastConversation = ID;

    showNotification();
    updateCaption();
}

void MainWindow::loadSessionContent(QString ID)
{
    UserSession::SessionData& session = userSessionList.getSession(ID);
    session.active = true;

    // Load cached session content from session data
    int tabIndex = getSessionTabIndex(ID);
    QByteArray buffer;
    Wichat_addHTMLHeader(buffer, 0);
    buffer.append(session.cache);
    Wichat_addHTMLFooter(buffer, 0);
    browserList[tabIndex]->setHtml(renderHTML(buffer));

    // Load text input area from session data
    buffer.clear();
    //Wichat_addHTMLHeader(buffer, 1);
    buffer.append(session.input);
    //Wichat_addHTMLFooter(buffer, 1);
    editorList[tabIndex]->setHtml(buffer);
    applyFont();

    // Process possible events (redrawing etc.)
    QCoreApplication::processEvents();

    // Scroll both area to the end
    browserList[tabIndex]->verticalScrollBar()->setValue(
                    browserList[tabIndex]->verticalScrollBar()->maximum());
    editorList[tabIndex]->verticalScrollBar()->setValue(
                    editorList[tabIndex]->verticalScrollBar()->maximum());
}

void MainWindow::syncSessionContent(QString ID, bool closeSession)
{
    int index;

    if (ID.isEmpty())
    {
        index = ui->tabSession->currentIndex();
        if (index < 0)
           return;
        ID = browserList[index]->property("ID").toString();
    }

    index = getSessionIndex(ID);
    UserSession::SessionData& session = userSessionList.getSession(ID);
    if (session.ID == ID)
    {
        // Store content in input box to session cache
        session.input = editorList[index]->toHtml().toLatin1();
        session.active = !closeSession;
    }
}

QString MainWindow::renderHTML(const QByteArray& content)
{
    int p1, p2, p3, p4;
    QString temp;
    QString result = content;

    // Deal with emoticon
    p2 = 0;
    while(true)
    {
        p1 = result.indexOf("<emotion>", p2);
        p2 = result.indexOf("</emotion>", p1);
        if (p1 < 0 || p2 < 0)
            break;
        int emotionIndex = int(QString(result.mid(p1 + 9, p2 - p1 - 9))
                                      .toInt());
        // TODO: parse emoticon

        result.replace(p1, p2 - p1, temp);
    }

    // Deal with image
    p2 = 0;
    while(true)
    {
        p1 = result.indexOf("<file><type>i</type><name>", p2);
        p2 = result.indexOf("</name></file>", p1);
        if (p1 < 0 || p2 < 0)
            break;
        QString fileName = result.mid(p1 + 26, p2 - p1 - 26);
        temp = QString("<img src=""file://")
                    .append(fileName)
                    .append(""" />");

        result.replace(p1, p2 - p1, temp);
    }

    // Deal with file
    p2 = 0;
    while(true)
    {
        p1 = result.indexOf("<file><type>f</type><name>", p2);
        p2 = result.indexOf("</name></file>", p1);
        if (p1 < 0 || p2 < 0)
            break;
        QString fileName = result.mid(p1 + 26, p2 - p1 - 26);

        QString noteText;
        p3 = result.lastIndexOf("<div class=s", p1 - 1);
        p4 = result.lastIndexOf("<div class=r", p1 - 1);
        if (p3 >= 0 && p4  < p3)
            noteText = QString("You sent file ""%1"" to him/her.")
                              .arg(getFileNameFromPath(fileName));
        else
            noteText = QString("He/she sent file ""%1"" to you.")
                              .arg(getFileNameFromPath(fileName));
        temp = QString("<div style=""width:300px;border:1px solid;"">"
                          "<div style=""float:right"">")
                    .append(noteText)
                    .append("<a href=\"file://")
                    .append(fileName)
                    .append("\" target=_blank>View the file</a></div></div>");

        result.replace(p1, p2 - p1, temp);
    }

    return result;
}

void MainWindow::addTab(QString ID)
{
    QTextBrowser* newBrowser = new QTextBrowser;
    QTextEdit* newEditor = new QTextEdit;

    newBrowser->setProperty("ID", ID);
    newBrowser->setProperty("Initialized", false);
    newEditor->setProperty("ID", ID);
    newBrowser->setGeometry(ui->textBrowser->geometry());
    newEditor->setGeometry(ui->textEdit->geometry());

    browserList.push_back(newBrowser);
    editorList.push_back(newEditor);
    ui->tabSession->addTab(newBrowser,
                           QIcon(getStateImagePath(ID)),
                           ID);
    ui->frameTextGroup->layout()->addWidget(newEditor);
    ui->frameMsg->show();
}

// Initialize session tab with given session list
// Do not touch any session content
void MainWindow::loadTab()
{
    QList<QString> sessionIdList = userSessionList.sessionIdList();

    ui->tabSession->clear();

    for (int i=0; i<sessionIdList.count(); i++)
    {
        if (userSessionList.getSession(sessionIdList[i]).active)
            addTab(sessionIdList[i]);
    }
}

void MainWindow::refreshTab()
{
    for (int i=0; i<ui->tabSession->count(); i++)
    {
        ui->tabSession->setTabIcon(i, QIcon(getStateImagePath(
                                                ui->tabSession->tabText(i))));
    }
}

void MainWindow::highlightSession(QString ID, bool highlight)
{
    int index = getSessionTabIndex(ID);
    if (index < 0)
        return;
    if (highlight)
    {
        tabBarSession->setTabTextColor(index, QColor("#FFA07A"));
        setWindowTitle(QString("Message from %1").arg(ID));
        QApplication::alert(this);
    }
    else
    {
        tabBarSession->setTabTextColor(index, QColor(""));
        updateCaption();
    }
}

void MainWindow::removeTab(QString ID)
{
    syncSessionContent(ID, true);
    lastConversation.clear();

    // Actually close the tab
    int index = getSessionIndex(ID);
    browserList.removeAt(index);
    delete editorList[index];
    editorList.removeAt(index);
    ui->tabSession->removeTab(getSessionTabIndex(ID));
    if (editorList.count() < 1)
        ui->frameMsg->hide();
}

void MainWindow::changeSession()
{
    int queryID;
    globalAccount.resetSession(queryID);
}

void MainWindow::changeState(Account::OnlineState state)
{
    int queryID;
    globalAccount.setState(state, queryID);
}

void MainWindow::updateState()
{
    ui->tabSession->setTabIcon(getSessionTabIndex(userID),
                               QIcon(stateToImagePath(
                                            int(globalAccount.state()), true)));
    sysTrayIcon->setIcon(QIcon(stateToImagePath(
                                   int(globalAccount.state()), true)));
    for (int i=0; i<listFriendModel.rowCount(); i++)
    {
        if (listFriendModel.item(i, WICHAT_MAIN_FRILIST_FIELD_ID)
                            ->text() == userID)
        {
            listFriendModel.item(i, WICHAT_MAIN_FRILIST_FIELD_ICON)
                    ->setIcon(QIcon(stateToImagePath(
                                            int(globalAccount.state()), true)));
            break;
        }
    }
}

void MainWindow::updateCaption()
{
    QString title;
    if (lastConversation == userID || lastConversation.isEmpty())
    {
        title.append('[')
             .append(Wichat_stateToString(globalAccount.state()))
             .append(']');
        if (!globalAccount.offlineMsg().isEmpty())
            title.append('-').append(globalAccount.offlineMsg());
    }
    else
        title = QString("Session with %1")
                .arg(lastConversation);
    setWindowTitle(title);
}

void MainWindow::updateSysTrayMenu()
{
    QList<QAction*> actionList = menuSysTray->actions();
    if (actionList.isEmpty())
    {
        // Add a title in the tray menu
        QAction* action = new QAction(userID, menuSysTray);
        action->setData(0);
        menuSysTray->addAction(action);
        menuSysTray->addSeparator();

        // Add other options for switching online state
        for (int i=0; i<4; i++)
        {
            action = new QAction(menuSysTray);
            action->installEventFilter(this);
            action->setIcon(QIcon(stateToImagePath(int(Wichat_stateList[i]),
                                                   true)));
            action->setText(Wichat_stateToString(Wichat_stateList[i]));
            action->setData(int(Wichat_stateList[i]));
            menuSysTray->addAction(action);
        }

        // Add an entry for quitting program
        menuSysTray->addSeparator();
        action = new QAction("Quit", menuSysTray);
        action->setData(-1);
        menuSysTray->addAction(action);

        actionList = menuSysTray->actions();
    }
}

void MainWindow::updateFriendList()
{
    if (!ui->textFriendSearch->text().isEmpty())
        return;

    int queryID;
    globalAccount.queryFriendList(queryID);
}

QString MainWindow::getStateImagePath(QString ID)
{
    QString image = stateToImagePath(int(Account::OnlineState::Offline));
    if (ID == userID)
        image = stateToImagePath(int(globalAccount.state()), true);
    else
    {
        for (int i=0; i<listFriendModel.rowCount(); i++)
        {
            if (listFriendModel.item(i, WICHAT_MAIN_FRILIST_FIELD_ID)
                                ->text() == ID)
            {
                image = listFriendModel.item(i,
                                             WICHAT_MAIN_FRILIST_FIELD_ICONPATH)
                                        ->text();
                break;
            }
        }
    }
    return image;
}

bool MainWindow::isFriend(QString ID)
{
    bool found = false;
    for (int i=0; i<listFriendModel.rowCount(); i++)
    {
        if (listFriendModel.item(i, WICHAT_MAIN_FRILIST_FIELD_ID)->text() == ID)
        {
            found = true;
            break;
        }
    }
    return found;
}

void MainWindow::conversationLogin()
{
    switch (conversationLoginState)
    {
        case 0: // Not logged in
            globalConversation.verify(globalAccount.sessionID(),
                                  globalAccount.sessionKey());
            conversationLoginState = 1;
        case 1: // Waiting for server response
            // Do not use QEventloop here, as it might provoke deadlook
            break;
        case 2: // Logged in successfully
        default:
            break;
    }
}

void MainWindow::getMessageList()
{
    conversationLogin();
    globalConversation.getMessageList();
}

bool MainWindow::sendMessage(QString content, QString sessionID)
{
#ifndef QT_DEBUG
    if (globalAccount.state() == Account::OnlineState::None ||
        globalAccount.state() == Account::OnlineState::Offline)
    {
        QMessageBox::warning(this, "Please log in first",
                             "You are off-line now. "
                             "Please log in to send messages.");
        return false;
    }
#endif

    int sessionIndex = getSessionIndex(sessionID);
    if (sessionIndex < 0)
        return false;

    int queryID;
    QByteArray buffer(content.toUtf8());
    if (sessionID != userID)
    {
        if (!isFriend(sessionID))
        {
            QMessageBox::warning(this, "Unable send message",
                                 QString("You are not friend to %1.\n"
                                 "Please add %1 to your friend list before "
                                 "sending any message.")
                                 .arg(sessionID).arg(sessionID));
            return false;
        }
        conversationLogin();
        if (!globalConversation.sendMessage(sessionID,
                                            buffer,
                                            queryID))
        {
            QMessageBox::warning(this, "Cannot send message",
                                 "Wichat is currently unable to send message.");
            return false;
        }
        else
            queryList[queryID] = sessionID;
    }

    browserList[sessionIndex]->append(renderHTML(buffer));
    userSessionList.getSession(sessionID).cache.append(content);
    return true;
}

bool MainWindow::receiveMessage(QString sessionID)
{
    int queryID;
    conversationLogin();
    globalConversation.receiveMessage(sessionID, queryID);
    queryList[queryID] = sessionID;
    return true;
}

void MainWindow::showNotification()
{
    if (noteList.count() < 1)
        return;

    bool clearNote;
    static QList<Notification::Note> tempNoteList;
    tempNoteList = noteList.getAll();
    for (int i=0; i<tempNoteList.count(); i++)
    {
        clearNote = true; // The read note is removed by default
        switch (tempNoteList[i].type)
        {
            case Notification::FriendAdd:
            case Notification::FriendDelete:
                sysTrayNoteList->addNote(tempNoteList[i]);
                break;
            case Notification::GotMsg:
                if (tempNoteList[i].source == lastConversation)
                    receiveMessage(tempNoteList[i].source);
                else
                {
                    int index = getSessionIndex(tempNoteList[i].source);
                    if (index >= 0) // Session exists, highlight it
                    {
                        clearNote = false;
                        highlightSession(tempNoteList[i].source, true);
                    }
                    else // Otherwise, show notification only
                        sysTrayNoteList->addNote(tempNoteList[i]);
                }
                break;
            default:
                break;
        }
        if (clearNote)
            noteList.remove(tempNoteList[i].ID);
    }

    if (sysTrayNoteList->countNote() > 0)
    {
        // Check static indicator to avoid unnecessary operations
        // as this function is called frequently by timer
        if (notificationState == 0)
        {
            sysTrayIcon->setIcon(QIcon(":/Icons/notification.png"));
            notificationState = 1;
        }
    }
}

void MainWindow::fixBrokenConnection()
{
    int queryID;
    QString sessionID = brokenConnectionList.dequeue();
    conversationLogin();
    globalConversation.fixBrokenConnection(sessionID,
                                           queryID);
    queryList[queryID] = sessionID;
}

QString MainWindow::addSenderInfo(const QString& content, QString ID)
{
    QString configString;
    QString styleStringHeader, styleStringFooter;
    QString result("<div class=s><b>%ID%</b>&nbsp;&nbsp;%TIME%</div>");

    // Set up <font> tag
    configString = globalConfig.prefFontFamily(ID);
    if (!configString.isEmpty())
        styleStringHeader.append("face=\"")
                        .append(configString)
                        .append("\"");
    configString = globalConfig.prefFontColor(ID);
    if (!configString.isEmpty())
        styleStringHeader.append(" color=\"")
                        .append(configString)
                        .append("\"");
    configString = globalConfig.prefFontSize(ID);
    if (!configString.isEmpty())
        styleStringHeader.append("style=\"font-size:")
                        .append(configString)
                        .append("\"");
    if (!styleStringHeader.isEmpty())
    {
        styleStringHeader.prepend("<font ").append('>');
        styleStringFooter = "</font>";
    }
    configString = globalConfig.prefFontStyle(ID);
    if (configString == WICHAT_MAIN_FONT_STYLE_BOLD)
    {
        styleStringHeader.append("<b>");
        styleStringFooter.append("</b>");
    }
    else if (configString == WICHAT_MAIN_FONT_STYLE_ITALIC)
    {
        styleStringHeader.append("<i>");
        styleStringFooter.append("</i>");
    }
    else if (configString == WICHAT_MAIN_FONT_STYLE_UNDERLINE)
    {
        styleStringHeader.append("<u>");
        styleStringFooter.append("</u>");
    }
    else if (configString == WICHAT_MAIN_FONT_STYLE_STRIKEOUT)
    {
        styleStringHeader.append("<del>");
        styleStringFooter.append("</del>");
    }
    else if (configString == WICHAT_MAIN_FONT_STYLE_OVERLINE)
    {
        styleStringHeader.append("<span style=\"text-decoration:overline\">");
        styleStringFooter.append("</span>");
    }

    // Set up alignment tag
    configString = globalConfig.prefFontStyle(ID);
    if (configString == WICHAT_MAIN_TEXT_ALIGN_CENTER)
        styleStringHeader.prepend(" align=left>");
    else if (configString == WICHAT_MAIN_TEXT_ALIGN_RIGHT)
        styleStringHeader.prepend(" align=right>");
    else
        styleStringHeader.prepend(">");

    styleStringHeader.prepend("<div class=c");
    styleStringFooter.append("</div>");
    result.replace("%ID%", ID);
    result.replace("%TIME%", QDateTime::currentDateTime()
                                           .toString(WICHAT_MAIN_TIME_FORMAT));
    result.append(styleStringHeader).append(content).append(styleStringFooter);

    return result;
}

QString MainWindow::extractHTMLTag(const QString& rawHTML, QString tagName)
{
    int p1, p2, p3;
    QString tagBegin(tagName), tagEnd(tagName);
    tagBegin.prepend('<');
    tagEnd.prepend("</").append('>');

    p1 = rawHTML.indexOf(tagBegin, 0, Qt::CaseInsensitive);
    p2 = rawHTML.indexOf('>', p1);
    p3 = rawHTML.indexOf(tagEnd, 0, Qt::CaseInsensitive);
    if (p1 >= 0 && p2 > p1 && p3 > p2)
        return rawHTML.mid(p2 + 1, p3 - p2 - 1);
    else
        return "";
}

QString MainWindow::stateToImagePath(int stateNumber, bool displayHide)
{
    QString path = ":/Icons/";
    switch (Account::OnlineState(stateNumber))
    {
        case Account::OnlineState::Online:
            path.append("green");
            break;
        case Account::OnlineState::Busy:
            path.append("yellow");
            break;
        case Account::OnlineState::Hide:
            if (displayHide)
                path.append("blue");
            else
                path.append("grey");
            break;
        case Account::OnlineState::Offline:
        default:
            path.append("grey");
    }
    path.append(".ico");
    return path;
}

QString MainWindow::getFileNameFromPath(QString filePath)
{
    char pathSep;
    if (filePath.indexOf('/') >= 0)
        pathSep  = '/';
    else if (filePath.indexOf('\\') >= 0)
        pathSep  = '\\';
    else
        return filePath;
    return filePath.mid(filePath.lastIndexOf(pathSep) + 1);
}

void MainWindow::onChangeSessionFinished(int queryID, bool successful)
{
    Q_UNUSED(queryID)
    if (!successful)
        addTask(taskLogOut);
}

void MainWindow::onChangeStateFinished(int queryID,
                                       bool successful,
                                       Account::OnlineState newState)
{
    Q_UNUSED(queryID)
    Q_UNUSED(newState)
    if (!successful)
        QMessageBox::critical(this, "Wichat error",
                              "Cannot set online state.");
    else
        addTask(taskUpdateState);
}

void MainWindow::onUpdateFriendListFinished(int queryID,
                                    QList<Account::AccountListEntry> friends)
{
    Q_UNUSED(queryID)
    listFriendModel.removeRows(0, listFriendModel.rowCount());

    QList<QStandardItem*> row;
    QString iconPath;
    // Add an entry for current user

    iconPath = stateToImagePath(int(globalAccount.state()), true);
    row.append(new QStandardItem(QIcon(iconPath), userID));
    row.append(new QStandardItem(iconPath));
    row.append(new QStandardItem(userID));
    listFriendModel.appendRow(row);

    for (int i=0; i<friends.count(); i++)
    {
        row.clear();
        iconPath = stateToImagePath(int(friends[i].state));
        row.append(new QStandardItem(QIcon(iconPath), friends[i].ID));
        row.append(new QStandardItem(iconPath));
        row.append(new QStandardItem(friends[i].ID));
        listFriendModel.appendRow(row);

        // Update icons in tab
        int index = getSessionTabIndex(friends[i].ID);
        if (index >= 0)
            ui->tabSession->setTabIcon(index, QIcon(iconPath));
    }
}

void MainWindow::onAddFriendFinished(int queryID, bool successful)
{
    QString ID = queryList[queryID];
    if (ID.isEmpty())
        return;
    if (successful)
        QMessageBox::information(this, "Friend request sent",
                                 QString("Your friend request has been sent.\n"
                                         "Please wait respose from %1.")
                                 .arg(ID));
    else
        QMessageBox::warning(this, "Adding friend failed",
                             QString("Cannot add %1 to your friend list.")
                             .arg(ID));
}

void MainWindow::onRemoveFriendFinished(int queryID, bool successful)
{
    QString ID = queryList[queryID];
    if (ID.isEmpty())
        return;
    if (!successful)
        QMessageBox::warning(this, "Removing friend failed",
                             QString("Cannot remove %1 from your friend list.")
                             .arg(ID));
}

void MainWindow::onGetFriendInfoFinished(int queryID,
                                     QList<Account::AccountInfoEntry> infoList)
{
    Q_UNUSED(queryID)
    if (infoList.count() < 1)
        return;

    if (!accountInfo)
    {
        accountInfo = new AccountInfoDialog;
        connect(accountInfo,
                SIGNAL(remarksChanged(QString,QString)),
                this,
                SLOT(onFriendRemarksChanged(QString, QString)));
    }
    accountInfo->ID = infoList[0].ID;
    accountInfo->offlineMsg = infoList[0].offlineMsg;
    accountInfo->show();
}

void MainWindow::onFriendRequest(QString ID)
{
    Notification::Note note;
    note.ID = noteList.getNewID();
    note.source = ID;
    note.destination = userID;
    note.type = Notification::FriendAdd;
    noteList.append(note);
}

void MainWindow::onFriendRemoved(QString ID)
{
    Notification::Note note;
    note.ID = noteList.getNewID();
    note.source = ID;
    note.destination = userID;
    note.type = Notification::FriendDelete;
    noteList.append(note);
}

void MainWindow::onConnectionBroken(QString ID)
{
    brokenConnectionList.enqueue(ID);
    addTask(taskRebuildConnection);
}

void MainWindow::onConversationVerifyFinished(int queryID,
                                            Conversation::VerifyError errorCode)
{
    Q_UNUSED(queryID)
    if (errorCode == Conversation::VerifyError::Ok)
    {
        conversationLoginState = 2;
        peerSessionList.loadSessionKey(globalConfig.userDirectory(userID),
                                       globalConversation.keySalt());
    }
    else
        conversationLoginState = 0;
}

void MainWindow::onResetSessionFinished(int queryID, bool successful)
{
    Q_UNUSED(queryID)
    if (!successful)
        addTask(taskConversationLogin);
}

void MainWindow::onSendMessageFinished(int queryID, bool successful)
{
    if (successful)
        return;

    QString destination = queryList.value(queryID);
    queryList.remove(queryID);
    if (!destination.isEmpty())
        QMessageBox::warning(this, "Failed to send message",
                             QString("Wichat failed to send message to %1.\n"
                             "Please check your network, then try again.")
                             .arg(destination));
}

void MainWindow::onGetMessageListFinished(int queryID,
                                          QList<MessageListEntry> msgList)
{
    Q_UNUSED(queryID)
    Notification::Note newNote;
    newNote.destination = userID;
    newNote.type = Notification::GotMsg;
    newNote.time = QDateTime::currentDateTime();
    for (int i=0; i<msgList.count(); i++)
    {
        newNote.source = msgList[i].ID;
        newNote.ID = noteList.getNewID();
        noteList.append(newNote);
    }
}

void MainWindow::onReceiveMessageFinished(int queryID, QByteArray& content)
{
    QString sourceID = queryList.value(queryID);
    if (sourceID.isEmpty())
        return;
    queryList.remove(queryID);

    content.replace("<div class=s>", "<div class=r>");
    userSessionList.getSession(sourceID).cache.append(content);
    browserList[getSessionIndex(sourceID)]->append(renderHTML(content));
}

void MainWindow::onTimerTimeout()
{
    static int count;
    count++;

    if (count % WICHAT_MAIN_TIMER_INTERVAL_GETFRIEND == 0)
        addTask(taskUpdateFriList);
    if (count % WICHAT_MAIN_TIMER_INTERVAL_GETMSG == 0)
        addTask(taskGetMsgList);
    if (count % WICHAT_MAIN_TIMER_INTERVAL_SHOWNOTE == 0)
        addTask(taskShowNotification);

    doTask();
}

void MainWindow::onMouseButtonRelease()
{
    ui->frameFont->hide();
    emoticonList->hide();
}

void MainWindow::onMouseHoverEnter(QObject* watched, QHoverEvent* event)
{
    Q_UNUSED(event)
    if (watched == tabBarSession)
    {
        int index = tabBarSession->tabAt(QCursor::pos()
                                                    - pos()
                                                    - QPoint(0, 50));
        if (index < 0)
            return;

        lastHoveredTab = index;
        tabBarSession->setTabButton(index,
                                               QTabBar::RightSide,
                                               buttonTabClose);
    }
}

void MainWindow::onMouseHoverLeave(QObject* watched, QHoverEvent* event)
{
    Q_UNUSED(event)
    if (watched == tabBarSession && lastHoveredTab >= 0)
    {
        tabBarSession->setTabButton(lastHoveredTab,
                                               QTabBar::RightSide,
                                               nullptr);
        lastHoveredTab = -1;
    }
}

void MainWindow::onKeyRelease(QObject* watched, QKeyEvent* event)
{
    Q_UNUSED(watched)
    if (event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter)
    {
        if (lastConversation.isEmpty())
            return;
        if (editorList[getSessionIndex(lastConversation)]->hasFocus())
        {
            if ((globalConfig.prefSendKey(userID) ==
                                WICHAT_MAIN_EDITOR_SENDKEY_ENTER &&
                 event->modifiers() == Qt::NoModifier) ||
                (globalConfig.prefSendKey(userID) ==
                                WICHAT_MAIN_EDITOR_SENDKEY_CTRLENTER &&
                 event->modifiers() == Qt::ControlModifier))
            {
                on_buttonSend_clicked();
            }
        }
    }
}

void MainWindow::onSessionTabClose(bool checked)
{
    Q_UNUSED(checked)
    int index = tabBarSession->tabAt(QCursor::pos()
                                                - pos()
                                                - QPoint(0, 50));
    tabBarSession->setTabButton(index,
                                           QTabBar::RightSide,
                                           nullptr);
    removeTab(ui->tabSession->widget(index)->property("ID").toString());
    updateCaption();
}

void MainWindow::onSysTrayNoteClicked(const Notification::Note& note)
{
    int queryID;
    switch (note.type)
    {
        case Notification::FriendAdd:
            if (QMessageBox::question(this, "New friend request",
                                      QString("%1 wants to add you as friend. "
                                      "Do you accept this request?")
                                      .arg(note.source),
                                      QMessageBox::Yes | QMessageBox::No)
                    == QMessageBox::Yes)
                globalAccount.addFriend(note.source, queryID);
            else
                globalAccount.removeFriend(note.source, queryID);
            break;
        case Notification::FriendDelete:
            QMessageBox::information(this, "Friend removed",
                                     QString("%1 remove you from his/hers "
                                             "friend list.")
                                     .arg(note.source));
            globalAccount.removeFriend(note.source, queryID);
            break;
        case Notification::GotMsg:
            loadSession(note.source, true);
            setWindowState(Qt::WindowActive);
            break;
        default:;
    }
    sysTrayNoteList->removeNote(note.ID);

    if (sysTrayNoteList->countNote() < 1)
    {
        notificationState = 0;
        sysTrayNoteList->hide();
        addTask(taskUpdateState);
    }
}

void MainWindow::onSysTrayIconClicked(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Context:
            // This does not work
            // updateSysTrayMenu();
            break;
        case QSystemTrayIcon::DoubleClick:
        case QSystemTrayIcon::Trigger:
            if (sysTrayNoteList->countNote() > 0)
            {
                sysTrayNoteList->show();
                sysTrayNoteList->activate();
                QRect iconPos = sysTrayIcon->geometry();
                sysTrayNoteList->move(iconPos.x() -
                                      sysTrayNoteList->width() / 2,
                                      iconPos.y() -
                                      sysTrayNoteList->height());
            }
            else
                setWindowState(Qt::WindowActive);
            break;
        default:;
    }
}

void MainWindow::onSysTrayMenuShowed()
{
    updateSysTrayMenu();
}

void MainWindow::onSysTrayMenuClicked(QAction* action)
{
    int state = action->data().toInt();
    if (state == -1)
        close();
    else if (state > 0)
        changeState(Account::OnlineState(action->data().toInt()));
}

void MainWindow::onEmoticonClicked(const QByteArray &emoticon)
{
    const uint* unicode = (const uint*)(emoticon.constData());
    int unicodeLength = emoticon.length() / sizeof(uint) +
                        (emoticon.length() % sizeof(uint) > 0);
    editorList[getSessionIndex(lastConversation)]->insertHtml(
                                            QString::fromUcs4(unicode,
                                                              unicodeLength));
}

void MainWindow::onListFriendMenuClicked(QAction *action)
{
    QModelIndexList index = ui->listFriend->selectionModel()->selectedIndexes();
    if (index.count() < 1)
        return;
    QString firstID = listFriendModel.item(index[0].row(),
                                            WICHAT_MAIN_FRILIST_FIELD_ID)
                                     ->text();

    QString actionType = action->data().toString();
    if (actionType == WICHAT_MAIN_MENU_FRIEND_OPEN)
    {
        on_listFriend_doubleClicked(index[0]);
    }
    else if (actionType == WICHAT_MAIN_MENU_FRIEND_REMOVE)
    {
        if (QMessageBox::information(this, "Remove a friend",
                                     QString("Do you really want to remove %1 "
                                             "from your friend list?")
                                            .arg(firstID),
                                     QMessageBox::Yes | QMessageBox::No)
                        != QMessageBox::Yes)
            return;
        int queryID;
        globalAccount.removeFriend(firstID, queryID);
        queryList[queryID] = firstID;
    }
    else if (actionType == WICHAT_MAIN_MENU_FRIEND_INFO)
    {
        int queryID;
        globalAccount.queryFriendInfo(firstID, queryID);
    }
}

void MainWindow::onFriendRemarksChanged(QString ID, QString remarks)
{
    int queryID;
    globalAccount.setFriendRemarks(ID, remarks, queryID);
}

void MainWindow::on_buttonFont_clicked()
{
    if (ui->frameFont->isVisible())
        ui->frameFont->hide();
    else
        ui->frameFont->show();
}

void MainWindow::on_buttonTextColor_clicked()
{
    if (!dialogColor)
        dialogColor = new QColorDialog;
    dialogColor->setCurrentColor(QColor(globalConfig.prefFontColor(userID)));
    dialogColor->exec();

    globalConfig.setPrefFontColor(userID,
                                  dialogColor->currentColor().name());
    applyFont();
}

void MainWindow::on_buttonTextStyle_clicked()
{
    QList<QAction*> actionList;
    QFont fontStyle;
    QStringList fontStyleArgs;

    if (!menuFontStyle)
    {
        // Create font style menu with previewed effect
        menuFontStyle = new QMenu;

        actionList.append(menuFontStyle->addAction("Bold"));
        actionList[0]->setCheckable(true);
        actionList[0]->setData(WICHAT_MAIN_FONT_STYLE_BOLD);
        fontStyle.setBold(true);
        actionList[0]->setFont(fontStyle);
        fontStyle.setBold(false);

        actionList.append(menuFontStyle->addAction("Italic"));
        actionList[1]->setCheckable(true);
        actionList[1]->setData(WICHAT_MAIN_FONT_STYLE_ITALIC);
        fontStyle.setItalic(true);
        actionList[1]->setFont(fontStyle);
        fontStyle.setItalic(false);

        actionList.append(menuFontStyle->addAction("Underline"));
        actionList[2]->setCheckable(true);
        actionList[2]->setData(WICHAT_MAIN_FONT_STYLE_UNDERLINE);
        fontStyle.setUnderline(true);
        actionList[2]->setFont(fontStyle);
        fontStyle.setUnderline(false);

        actionList.append(menuFontStyle->addAction("StrikeOut"));
        actionList[3]->setCheckable(true);
        actionList[3]->setData(WICHAT_MAIN_FONT_STYLE_STRIKEOUT);
        fontStyle.setStrikeOut(true);
        actionList[3]->setFont(fontStyle);
        fontStyle.setStrikeOut(false);

        actionList.append(menuFontStyle->addAction("Overline"));
        actionList[4]->setCheckable(true);
        actionList[4]->setData(WICHAT_MAIN_FONT_STYLE_OVERLINE);
        fontStyle.setOverline(true);
        actionList[4]->setFont(fontStyle);
        fontStyle.setOverline(false);
    }
    else
    {
        // Parse font style arguments from user config
        fontStyleArgs = globalConfig.prefFontStyle(userID)
                                    .split(',').toVector().toList();
        actionList = menuFontStyle->actions();
        for (int i=0; i<actionList.count(); i++)
        {
            if (fontStyleArgs.contains(actionList[i]->data().toString()))
                actionList[0]->setChecked(true);
            else
                actionList[0]->setChecked(false);
        }
    }

    menuFontStyle->popup(QCursor::pos());

    // Store new style to user config
    fontStyleArgs.clear();
    for (int i=0; i< actionList.count(); i++)
    {
        if (actionList[i]->isChecked())
            fontStyleArgs.push_back(actionList[i]->data().toString());
    }
    globalConfig.setPrefFontFamily(userID, fontStyleArgs.join(","));
    applyFont();
}

void MainWindow::on_buttonTextAlign_clicked()
{
    QList<QAction*> actionList;
    QString alignMode;

    if (!menuTextAlign)
    {
        // Create align style menu with icons
        menuTextAlign = new QMenu;

        actionList.append(menuTextAlign->addAction("Left justify"));
        actionList[0]->setCheckable(true);
        actionList[0]->setData(WICHAT_MAIN_TEXT_ALIGN_LEFT);
        actionList[0]->setIcon(QIcon(":/Icons/format-justify-left.png"));

        actionList.append(menuTextAlign->addAction("Center justify"));
        actionList[1]->setCheckable(true);
        actionList[1]->setData(WICHAT_MAIN_TEXT_ALIGN_CENTER);
        actionList[1]->setIcon(QIcon(":/Icons/format-justify-center.png"));

        actionList.append(menuTextAlign->addAction("Right justify"));
        actionList[2]->setCheckable(true);
        actionList[2]->setData(WICHAT_MAIN_TEXT_ALIGN_RIGHT);
        actionList[2]->setIcon(QIcon(":/Icons/format-justify-right.png"));

        groupTextAlign = new QActionGroup(menuTextAlign);
        for (int i=0; i<actionList.count(); i++)
            groupTextAlign->addAction(actionList[i]);
    }
    else
    {
        // Parse align style from user config
        alignMode = globalConfig.prefTextAlign(userID);
        actionList = groupTextAlign->actions();
        if (alignMode == WICHAT_MAIN_TEXT_ALIGN_CENTER)
            actionList[1]->setChecked(true);
        else if (alignMode == WICHAT_MAIN_TEXT_ALIGN_RIGHT)
            actionList[2]->setChecked(true);
        else
            actionList[0]->setChecked(true);
    }

    menuTextAlign->popup(QCursor::pos());

    // Store new style to user config
    alignMode.clear();
    for (int i=0; i< actionList.count(); i++)
    {
        if (actionList[i]->isChecked())
        {
            alignMode = actionList[i]->data().toString();
            break;
        }
    }
    globalConfig.setPrefTextAlign(userID, alignMode);
    applyFont();
}

void MainWindow::on_comboFontFamily_currentTextChanged(const QString &arg1)
{
    globalConfig.setPrefFontFamily(userID, arg1);
    applyFont();
}

void MainWindow::on_comboTextSize_currentTextChanged(const QString &arg1)
{
    globalConfig.setPrefFontSize(userID, arg1);
    applyFont();
}

void MainWindow::on_buttonSend_clicked()
{
    QString content;
    int sessionIndex = getSessionIndex(lastConversation);
    content = addSenderInfo(extractHTMLTag(editorList[sessionIndex]->toHtml(),
                                           "body"),
                            userID);
    if (sendMessage(content, lastConversation))
        editorList[sessionIndex]->clear();
}

void MainWindow::on_buttonSendOpt_clicked()
{
    QList<QAction*> actionList;
    int keyCode;

    if (!menuSendOption)
    {
        // Create send option menu
        menuSendOption = new QMenu;
        groupSendOption = new QActionGroup(menuSendOption);

        actionList.append(menuSendOption->addAction("Send with Enter key"));
        actionList[0]->setCheckable(true);
        actionList[0]->setData(WICHAT_MAIN_EDITOR_SENDKEY_ENTER);

        actionList.append(menuSendOption->addAction("Send with Ctrl+Enter key"));
        actionList[1]->setCheckable(true);
        actionList[1]->setData(WICHAT_MAIN_EDITOR_SENDKEY_CTRLENTER);

        groupSendOption = new QActionGroup(menuSendOption);
        for (int i=0; i<actionList.count(); i++)
            groupSendOption->addAction(actionList[i]);
    }
    else
    {
        // Parse send option from user config
        keyCode = globalConfig.prefSendKey(userID);
        actionList = groupSendOption->actions();
        if (keyCode == WICHAT_MAIN_EDITOR_SENDKEY_CTRLENTER)
            actionList[1]->setChecked(true);
        else
            actionList[0]->setChecked(true);
    }

    menuSendOption->popup(QCursor::pos());

    // Store new style to user config
    keyCode = WICHAT_MAIN_EDITOR_SENDKEY_ENTER;
    for (int i=0; i< actionList.count(); i++)
    {
        if (actionList[i]->isChecked())
        {
            keyCode = actionList[i]->data().toInt();
            break;
        }
    }
    globalConfig.setPrefSendKey(userID, keyCode);
}

void MainWindow::on_tabSession_tabBarClicked(int index)
{
    Q_UNUSED(index)
    onMouseHoverEnter(tabBarSession, nullptr);
}


void MainWindow::on_tabSession_currentChanged(int index)
{
    if (index < 0)
        return;

    QString ID = ui->tabSession->widget(index)->property("ID").toString();
    loadSession(ID, false);
}

void MainWindow::on_listFriend_doubleClicked(const QModelIndex &index)
{
    loadSession(listFriendModel.item(index.row(), WICHAT_MAIN_FRILIST_FIELD_ID)
                                    ->text());
}

void MainWindow::on_listFriend_customContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos)
    if (menuFriendOption->actions().count() < 1)
    {
        QAction* action = new QAction(menuFriendOption);
        action->setText("Open dialog");
        action->setData(WICHAT_MAIN_MENU_FRIEND_OPEN);
        menuFriendOption->addAction(action);

        action = new QAction(menuFriendOption);
        action->setText("Remove this friend");
        action->setData(WICHAT_MAIN_MENU_FRIEND_REMOVE);
        menuFriendOption->addAction(action);

        action = new QAction(menuFriendOption);
        action->setText("View information");
        action->setData(WICHAT_MAIN_MENU_FRIEND_INFO);
        menuFriendOption->addAction(action);
    }
    menuFriendOption->popup(QCursor::pos());
}

void MainWindow::on_buttonImage_clicked()
{
    QString path = QFileDialog::getOpenFileName(this,
                         "Send image(s)",
                         lastFilePath,
                         QString(WICHAT_MAIN_FILE_FILTER_GIF).append(";;")
                         .append(WICHAT_MAIN_FILE_FILTER_JPG).append(";;")
                         .append(WICHAT_MAIN_FILE_FILTER_PNG),
                         &lastImageFilter);
    if (path.isEmpty())
        return;

    QString content("<file><type>i</type><name>%FILE%</name></file>");
    content.replace("%FILE%", path);
    sendMessage(addSenderInfo(content, userID), lastConversation);
    lastFilePath = path;
}

void MainWindow::on_buttonFile_clicked()
{
    QString path = QFileDialog::getOpenFileName(this,
                         "Send image(s)",
                         lastFilePath,
                         QString(WICHAT_MAIN_FILE_FILTER_ALL));
    if (path.isEmpty())
        return;

    QString content("<file><type>f</type><name>%FILE%</name></file>");
    content.replace("%FILE%", path);
    sendMessage(addSenderInfo(content, userID), lastConversation);
    lastFilePath = path;
}

void MainWindow::on_textFriendSearch_textChanged(const QString &arg1)
{
    if (arg1.isEmpty())
    {
        ui->labelFriendSearch->show();
        for (int i=0; i<listFriendModel.rowCount(); i++)
            ui->listFriend->setRowHidden(i ,false);
    }
    else
    {
        ui->labelFriendSearch->hide();
        for (int i=0; i<listFriendModel.rowCount(); i++)
        {
            if (listFriendModel.item(i, WICHAT_MAIN_FRILIST_FIELD_ID)->text()
                               .indexOf(arg1) >= 0)
                ui->listFriend->setRowHidden(i, false);
            else
                ui->listFriend->setRowHidden(i, true);
        }
    }
}

void MainWindow::on_buttonFriendAdd_clicked()
{
    QString ID = ui->textFriendSearch->text();
    if (ID.isEmpty())
        return;

    int queryID;
    if (QMessageBox::question(this, "Add friend",
                              QString("Do you want to add %1 as friend?")
                              .arg(ID),
                              QMessageBox::Yes | QMessageBox::No)
            == QMessageBox::Yes)
    {
        globalAccount.addFriend(ID, queryID);
        queryList[queryID] = ID;
    }
}

void MainWindow::on_buttonEmotion_clicked()
{
    if (emoticonList->isVisible())
        emoticonList->hide();
    else
    {
        emoticonList->move(ui->frameTextCtrl->pos() +
                           ui->buttonEmotion->pos() -
                           QPoint(0, emoticonList->height()));
        emoticonList->show();
    }
}
