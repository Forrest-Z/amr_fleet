//
// Created by jakub on 22. 3. 2020.
//

#ifndef SRC_MONITORWIDGET_H
#define SRC_MONITORWIDGET_H

#include <QWidget>
#include <QtWidgets>




namespace amr_gui{
class MonitorWidget : public QWidget {
        Q_OBJECT
    public:
        explicit MonitorWidget(QWidget *parent = nullptr);

    private slots:

    private:
        void setupActions();


    };

}
#endif //SRC_MONITORWIDGET_H
