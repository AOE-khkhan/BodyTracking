//
//  Jacobian.cpp
//  BodyTracking
//
//  Created by Philipp Achenbach on 16.05.18.
//  Copyright © 2018 KTX Software Development. All rights reserved.
//

#include "pch.h"
#include "Jacobian.h"

#include "RotationUtility.h"
#include "MeshObject.h"

#include "Jacobian/MatrixRmn.h"

#include <Kore/Log.h>

Jacobian::Jacobian(BoneNode* targetBone, Kore::vec4 pos, Kore::Quaternion rot) {
    endEffektor = targetBone;
    pos_soll = pos;
    rot_soll = rot;
    
    /* BoneNode* bone = targetBone;
    int counter = 0;
    while (bone->initialized) {
        Kore::vec3 axes = bone->axes;
        
        if (axes.x() == 1.0) counter += 1;
        if (axes.y() == 1.0) counter += 1;
        if (axes.z() == 1.0) counter += 1;
        
        bone = bone->parent;
    }
    log(Kore::Info, "Die Anzahl der Gelenke-Freiheitsgrade ist %i", counter); */
}

float Jacobian::getError() {
    return (!deltaP.isZero() ? deltaP : calcDeltaP()).getLength();
}

Jacobian::vec_n Jacobian::calcDeltaThetaByTranspose() {
    return calcJacobian().Transpose() * calcDeltaP();
}

Jacobian::vec_n Jacobian::calcDeltaThetaByPseudoInverse() {
    return calcPseudoInverse(lambdaPseudoInverse) * calcDeltaP();
}

Jacobian::vec_n Jacobian::calcDeltaThetaByDLS() {
    return calcPseudoInverse(lambdaDLS) * calcDeltaP();
}

Jacobian::vec_n Jacobian::calcDeltaThetaBySVD() {
    calcSVD();
    
    mat_nxm D;
    for (int i = 0; i < Min(nDOFs, nJointDOFs); ++i)
        D[i][i] = 1 / (d[i] + lambdaPseudoInverse);
        // D[i][i] = d[i] > lambdaPseudoInverse ? 1 / d[i] : 0;
    
    return V * D * U.Transpose() * calcDeltaP();
}

Jacobian::vec_n Jacobian::calcDeltaThetaByDLSwithSVD() {
    calcSVD();
    
    mat_nxm E;
    for (int i = 0; i < Min(nDOFs, nJointDOFs); ++i)
        E[i][i] = d[i] / (d[i] * d[i] + lambdaDLS * lambdaDLS);
    
    return V * E * U.Transpose() * calcDeltaP();
}

Jacobian::vec_n Jacobian::calcDeltaThetaBySDLS() {
    return calcPseudoInverse(lambdaDLS) * calcDeltaP();
}

// ################################################################

Jacobian::vec_m Jacobian::calcDeltaP() {
    vec_m deltaP;
    
    // Calculate difference between desired position and actual position of the end effector
    Kore::vec3 deltaPos = pos_soll - calcPosition(endEffektor);
    if (nDOFs > 0) deltaP[0] = deltaPos.x();
    if (nDOFs > 1) deltaP[1] = deltaPos.y();
    if (nDOFs > 2) deltaP[2] = deltaPos.z();
    
    // Calculate difference between desired rotation and actual rotation
    if (nDOFs > 3) {
        Kore::vec3 deltaRot = Kore::vec3(0, 0, 0);
        Kore::Quaternion rot_aktuell;
        Kore::RotationUtility::getOrientation(&endEffektor->combined, &rot_aktuell);
        Kore::Quaternion desQuat = rot_soll;
        desQuat.normalize();
        Kore::Quaternion quatDiff = desQuat.rotated(rot_aktuell.invert());
        if (quatDiff.w < 0) quatDiff = quatDiff.scaled(-1);
        Kore::RotationUtility::quatToEuler(&quatDiff, &deltaRot.x(), &deltaRot.y(), &deltaRot.z());
        
        deltaP[3] = deltaRot.x();
        if (nDOFs > 4) deltaP[4] = deltaRot.y();
        if (nDOFs > 5) deltaP[5] = deltaRot.z();
    }
    
    return deltaP;
}

Jacobian::mat_mxn Jacobian::calcJacobian() {
    Jacobian::mat_mxn jacobianMatrix;
    BoneNode* targetBone = endEffektor;
    
    // Get current rotation and position of the end-effector
    Kore::vec3 p_aktuell = calcPosition(targetBone);
    
    int joint = 0;
    while (targetBone->initialized) {
        Kore::vec3 axes = targetBone->axes;
        
        if (axes.x() == 1.0) {
            vec_m column = calcJacobianColumn(targetBone, p_aktuell, Kore::vec3(1, 0, 0));
            for (int i = 0; i < nDOFs; ++i) jacobianMatrix[i][joint] = column[i];
            joint += 1;
        }
        if (axes.y() == 1.0) {
            vec_m column = calcJacobianColumn(targetBone, p_aktuell, Kore::vec3(0, 1, 0));
            for (int i = 0; i < nDOFs; ++i) jacobianMatrix[i][joint] = column[i];
            joint += 1;
        }
        if (axes.z() == 1.0) {
            vec_m column = calcJacobianColumn(targetBone, p_aktuell, Kore::vec3(0, 0, 1));
            for (int i = 0; i < nDOFs; ++i) jacobianMatrix[i][joint] = column[i];
            joint += 1;
        }
        
        targetBone = targetBone->parent;
    }
    
    return jacobianMatrix;
}

Jacobian::vec_m Jacobian::calcJacobianColumn(BoneNode* bone, Kore::vec3 p_aktuell, Kore::vec3 rotAxis) {
    vec_m column;
    
    // Get rotation and position vector of the current bone
    Kore::vec3 p_j = calcPosition(bone);
    
    // get rotation-axis
    Kore::vec4 v_j = bone->combined * Kore::vec4(rotAxis.x(), rotAxis.y(), rotAxis.z(), 0);
    
    // cross-product
    Kore::vec3 pTheta = Kore::vec3(v_j.x(), v_j.y(), v_j.z()).cross(p_aktuell - p_j);
    
    if (nDOFs > 0) column[0] = pTheta.x();
    if (nDOFs > 1) column[1] = pTheta.y();
    if (nDOFs > 2) column[2] = pTheta.z();
    if (nDOFs > 3) column[3] = v_j.x();
    if (nDOFs > 4) column[4] = v_j.y();
    if (nDOFs > 5) column[5] = v_j.z();
    
    return column;
}

Jacobian::mat_nxm Jacobian::calcPseudoInverse(float lambda) { // lambda != 0 => DLS!
    Jacobian::mat_mxn jacobian = calcJacobian();
    
    if (nDOFs <= nJointDOFs) { // m <= n
        // Left Damped pseudo-inverse
        return (jacobian.Transpose() * jacobian + Jacobian::mat_nxn::Identity() * lambda * lambda).Invert() * jacobian.Transpose();
    }
    else {
        // Right Damped pseudo-inverse
        return jacobian.Transpose() * (jacobian * jacobian.Transpose() + Jacobian::mat_mxm::Identity() * lambda * lambda).Invert();
    }
}

Kore::vec3 Jacobian::calcPosition(BoneNode* bone) {
    Kore::vec3 result;
    
    // from quat to euler!
    Kore::vec4 quat = bone->combined * Kore::vec4(0, 0, 0, 1);
    quat *= 1.0 / quat.w();
    
    result.x() = quat.x();
    result.y() = quat.y();
    result.z() = quat.z();
    
    return result;
}

void Jacobian::calcSVD() {
    MatrixRmn J = MatrixRmn(nDOFs, nJointDOFs);
    MatrixRmn U = MatrixRmn(nDOFs, nDOFs);
    MatrixRmn V = MatrixRmn(nJointDOFs, nJointDOFs);
    VectorRn d = VectorRn(Min(nDOFs, nJointDOFs));
    
    Jacobian::mat_mxn jacobian = calcJacobian();
    for (int m = 0; m < nDOFs; ++m) {
        for (int n = 0; n < nJointDOFs; ++n) {
            J.Set(m, n, (double) jacobian[m][n]);
        }
    }
    
    J.ComputeSVD(U, d, V);
    assert(J.DebugCheckSVD(U, d , V));
    
    for (int m = 0; m < Max(nDOFs, nJointDOFs); ++m) {
        for (int n = 0; n < Max(nDOFs, nJointDOFs); ++n) {
            if (m < nDOFs && n < nDOFs)
                Jacobian::U[m][n] = (float) U.Get(m, n);
            
            if (m < nJointDOFs && n < nJointDOFs)
                Jacobian::V[m][n] = (float) V.Get(m, n);
            
            if (m == n && m < Min(nDOFs, nJointDOFs))
                Jacobian::d[m] = (float) d.Get(m);
        }
    }
}