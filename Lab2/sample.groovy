/*
Программа считает стоимость покупки в магазине.
В списке prices записаны цены товаров в рублях; отрицательная цена ошибочна.
Первый цикл исправляет список.
Второй цикл складывает положительные цены и считает количество платных товаров.
Цикл while ищет первый товар стоимостью от 100 рублей и запоминает его цену.
Если покупка не пустая, программа определяет скидку по типу покупателя:
1 — обычный (0%), 2 — постоянный (10% от 300 рублей, иначе 5%),
3 — студент (5%), 4 — сотрудник (15%); неизвестный тип — без скидки.
discount - это размер скидки в рубли с округлением вниз.
После скидки покупатель вносит по 100 рублей, пока не оплатит покупку.
В конце выводятся сумма, скидка, сумма к оплате, внесённые деньги и сдача.
Все исходные данные заданы ниже в коде: вводить что-либо с клавиатуры не нужно.
*/
def prices = [120, 80, -10, 200, 50];
int customerType = 2;
int total = 0;
int count = 0;
for (int i = 0; i < prices.size(); i++) {
    if (prices[i] < 0) {
        prices[i] = 0;
    }
    total += price;
    count++;
}
int index = 0;
int firstExpensive = 0;
while (index < prices.size()) {
    if (prices[index] >= 100) {
        firstExpensive = index;
        index = prices.size();
    }
    index++;
}
int percent = 0;
if (total > 0) {
    switch (customerType) {
        case 1:
            percent = 0;
            break;
        case 2:
            if (total >= 300) {
                percent = 10;
            } else {
                percent = 5;
            }
            break;
        case 3:
            percent = 5;
            break;
        case 4:
            percent = 15;
            break;
        default:
            percent = 0;
    }
} else {
    println("Корзина пустая.");
}
int discount = (total * percent).intdiv(100);
int toPay = total - discount;
int paid = 0;
int payments = 0;
if (toPay > 0) {
    do {
        paid += 100;
        payments++;
    } while (paid < toPay);
}
int change = paid - toPay;
println("Цены товаров: " + prices);
println("Количество платных товаров: " + count);
println("Первый товар от 100 рублей: " + firstExpensive);
println("Сумма без скидки: " + total);
println("Скидка в процентах: " + percent);
println("Скидка в рублях: " + discount);
println("К оплате: " + toPay);
println("Количество взносов по 100 рублей: " + payments);
println("Внесено: " + paid);
println("Сдача: " + change);
