/*
    This file is part of the Zero Reserve Plugin for Retroshare.

    Zero Reserve is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Zero Reserve is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Zero Reserve.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef REMOTEPAYMENTREQUESTDIALOG_H
#define REMOTEPAYMENTREQUESTDIALOG_H

#include <QDialog>

namespace Ui {
class RemotePaymentRequestDialog;
}

class RemotePaymentRequestDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit RemotePaymentRequestDialog(QWidget *parent = 0);
    ~RemotePaymentRequestDialog();

private slots:
    void currencySelected( QString currency );
    void amountEntered(QString amount );
    void sendRequest();

private:
    QString getPayAddress( QString amount, QString currency );
    
private:
    Ui::RemotePaymentRequestDialog *ui;
};

#endif // REMOTEPAYMENTREQUESTDIALOG_H
